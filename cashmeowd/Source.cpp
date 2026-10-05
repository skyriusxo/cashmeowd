#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <conio.h>
using namespace std;

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
    }

    if (hToken) CloseHandle(hToken);
    return isAdmin;
}

void SelfElevate() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOW;

    if (ShellExecuteExW(&sei)) {
        exit(0);
    }
    else {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            cout << "elevation cancelled. exiting." << endl;
        }
        else {
            cout << "elevation failed. error: " << err << endl;
        }
    }
}

struct GameProfile {
    const wchar_t* exeName;
    uintptr_t ptr1;
    const wchar_t* version;
};

const GameProfile profiles[]={
    {L"modded.exe", 0x62B600, L"modded"},
    {L"game.dat", 0x6395A0, L"ea"},
    {L"game.dat", 0x6395A0, L"steam"},
};


uintptr_t GetModuleBaseAddress(DWORD processId, const wchar_t* modName = nullptr) {
    uintptr_t modBaseAddr = 0;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

    if (hProcess) {
        HMODULE hMods[1024];
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            size_t moduleCount = cbNeeded / sizeof(HMODULE);

            for (size_t i = 0; i < moduleCount; i++) {
                if (modName == nullptr && i == 0) {
                    modBaseAddr = (uintptr_t)hMods[i];
                    break;
                }

                if (modName != nullptr) {
                    wchar_t szModName[MAX_PATH];
                    if (GetModuleBaseNameW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
                        if (_wcsicmp(szModName, modName) == 0) {
                            modBaseAddr = (uintptr_t)hMods[i];
                            break;
                        }
                    }
                }
            }
        }
        CloseHandle(hProcess);
    }
    return modBaseAddr;
}

DWORD FindProcessId(const wstring& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);

    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (processesSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    Process32First(processesSnapshot, &processInfo);
    do {
        if (_wcsicmp(processName.c_str(), processInfo.szExeFile) == 0) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    } while (Process32Next(processesSnapshot, &processInfo));

    CloseHandle(processesSnapshot);
    return 0;
}


void memoryTamper(DWORD processId, uintptr_t baseAddr, int cash, uintptr_t ptr1) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processId);
    if (hProcess == NULL) {
        cout << L"failed to open process. error code: " << GetLastError() << endl;
        return;
    }

    uintptr_t p1 = 0;
    ReadProcessMemory(hProcess, (LPCVOID)(baseAddr + ptr1), &p1, sizeof(p1), NULL);

    uintptr_t playerPtr = 0;
    ReadProcessMemory(hProcess, (LPCVOID)(p1 + 0x0C), &playerPtr, sizeof(playerPtr), NULL);

    while (playerPtr == 0) {
        Sleep(100);
        ReadProcessMemory(hProcess, (LPCVOID)(p1 + 0x0C), &playerPtr, sizeof(playerPtr), NULL);
    }

    uintptr_t cashAddress = playerPtr + 0x38;

    WriteProcessMemory(hProcess, (LPVOID)cashAddress, &cash, sizeof(cash), NULL);

    WriteProcessMemory(hProcess, (LPVOID)cashAddress, &cash, sizeof(cash), NULL);
    cout << "cash set to " << cash << " ^_^ " << endl;
    Sleep(1000);

    CloseHandle(hProcess);
}

int main() {
    if (!IsRunAsAdmin()) {
        cout << "====================================================" << endl;
        cout << "        /\\_ /\\ " << endl;
        cout << "       ( T__T )  *sniff* " << endl;
        cout << "      +=========+ " << endl;
        cout << "      |  ADMIN  | " << endl;
        cout << "      |  PLEASE | " << endl;
        cout << "      +=========+ " << endl;
        cout << "====================================================" << endl;
        cout << "ERROR: run as admin !! press any keep to continue." << endl;
        cout << "====================================================" << endl;
        _getch();
        SelfElevate();
    }

    const GameProfile* profile = nullptr;
    DWORD processId = 0;

    for (auto& p : profiles) {
        processId = FindProcessId(p.exeName);
        if (processId != 0) {
            int count = 0;
            for (auto& other : profiles)
                if (_wcsicmp(other.exeName, p.exeName) == 0) count++;
            if (count == 1) {
                profile = &p;
                break;
            }

            cout << "Game.dat found nya~!! which version meow? :3" << endl;
            cout << "1 -  ea" << endl;
            cout << "2 -  steam" << endl;
            int choice;
            cin >> choice;

            profile = (choice == 1) ? &profiles[1] : &profiles[2];
            break;
        }
    }
    if (!profile) {
        cout << "ERROR: could not find game T_T" << endl;
        _getch();
        return 0;
    }

    int cash;

    while (true) {
        uintptr_t baseAddr = GetModuleBaseAddress(processId);

        cout << "type how much cash u want meow :3 : ";
        cin >> cash;
        memoryTamper(processId, baseAddr, cash, profile->ptr1);
    }
}
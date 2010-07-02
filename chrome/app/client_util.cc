// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Not generic, we only handle strings up to 128 chars.
bool ReadRegistryStr(HKEY key, const wchar_t* name, std::wstring* value) {
  BYTE out[128 * sizeof(wchar_t)];
  DWORD size = sizeof(out);
  DWORD type = 0;
  if (ERROR_SUCCESS != ::RegQueryValueExW(key, name, NULL, &type, out, &size))
    return false;
  if (type != REG_SZ)
    return false;
  value->assign(reinterpret_cast<wchar_t*>(out));
  return true;
}

// Gets chrome version according to the load path. |exe_path| must be the
// backslash terminated directory of the current chrome.exe.
bool GetVersion(const wchar_t* exe_path, const wchar_t* key_path,
                std::wstring* version) {
  HKEY reg_root = InstallUtil::IsPerUserInstall(exe_path) ? HKEY_CURRENT_USER :
                                                            HKEY_LOCAL_MACHINE;
  HKEY key;
  if (::RegOpenKeyEx(reg_root, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;

  // If 'new_chrome.exe' is present it means chrome was auto-updated while
  // running. We need to consult the opv value so we can load the old dll.
  // TODO(cpu) : This is solving the same problem as the environment variable
  // so one of them will eventually be deprecated.
  std::wstring new_chrome_exe(exe_path);
  new_chrome_exe.append(installer_util::kChromeNewExe);
  if (::PathFileExistsW(new_chrome_exe.c_str()) &&
      ReadRegistryStr(key, google_update::kRegOldVersionField, version)) {
    ::RegCloseKey(key);
    return true;
  }

  bool ret = ReadRegistryStr(key, google_update::kRegVersionField, version);
  ::RegCloseKey(key);
  return ret;
}

// Gets the path of the current exe with a trailing backslash.
std::wstring GetExecutablePath() {
  wchar_t path[MAX_PATH];
  ::GetModuleFileNameW(NULL, path, MAX_PATH);
  if (!::PathRemoveFileSpecW(path))
    return std::wstring();
  std::wstring exe_path(path);
  return exe_path.append(L"\\");
}

// Not generic, we only handle strings up to 128 chars.
bool EnvQueryStr(const wchar_t* key_name, std::wstring* value) {
  wchar_t out[128];
  DWORD count = sizeof(out)/sizeof(out[0]);
  DWORD rv = ::GetEnvironmentVariableW(key_name, out, count);
  if ((rv == 0) || (rv >= count))
    return false;
  *value = out;
  return true;
}

bool IsRunningHeadless() {
  return (0 != ::GetEnvironmentVariableW(L"CHROME_HEADLESS", NULL, 0));
}

// Expects that |dir| has a trailing backslash. |dir| is modified so it
// contains the full path that was tried. Caller must check for the return
// value not being null to dermine if this path contains a valid dll.
HMODULE LoadChromeWithDirectory(std::wstring* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
#ifdef _WIN64
  if ((cmd_line.GetSwitchValueASCII(switches::kProcessType) ==
      switches::kNaClBrokerProcess) ||
      (cmd_line.GetSwitchValueASCII(switches::kProcessType) ==
      switches::kNaClLoaderProcess)) {
    // Load the 64-bit DLL when running in a 64-bit process.
    dir->append(installer_util::kChromeNaCl64Dll);
  } else {
    // Only NaCl broker and loader can be launched as Win64 processes.
    NOTREACHED();
    return NULL;
  }
#else
  dir->append(installer_util::kChromeDll);
#endif

  // Experimental pre-reading optimization
  // The idea is to pre read significant portion of chrome.dll in advance
  // so that subsequent hard page faults are avoided.
  DWORD pre_read_size_mb = 0;
  if (!cmd_line.HasSwitch(switches::kProcessType) &&
      (IsRunningHeadless() || InstallUtil::IsChromeFrameProcess())) {
    HKEY key = NULL;
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Google\\ChromeFrame",
                       0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
      DWORD unused = sizeof(pre_read_size_mb);
      if (::RegQueryValueEx(key, L"PreRead", NULL, NULL,
                            reinterpret_cast<LPBYTE>(&pre_read_size_mb),
                            &unused) != ERROR_SUCCESS) {
        pre_read_size_mb = 16; // Default
      }
      RegCloseKey(key);
      key = NULL;
    } else {
      pre_read_size_mb = 16; // Read in first 16 MB by default.
    }
  }

  if (pre_read_size_mb) {
    HANDLE chrome_dll = CreateFile(dir->c_str(),
                                       GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_FLAG_SEQUENTIAL_SCAN,
                                       NULL);
    if (chrome_dll == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      DLOG(ERROR) << __FUNCTION__ << " CreateFile( "
          << dir->c_str() << " ) failed. Error: " << error;
    } else {
      const size_t kChunkSize = 1024 * 1024; // 1 MB
      void* buffer = VirtualAlloc(NULL, kChunkSize, MEM_COMMIT, PAGE_READWRITE);
      if (buffer) {
        size_t read_size = pre_read_size_mb * kChunkSize;
        DWORD read = 0;
        while (::ReadFile(chrome_dll, buffer, kChunkSize, &read, NULL)) {
          // nothing to do here...
          read_size -= std::min(size_t(read), read_size);
          if (!read || !read_size)
            break;
        }

        VirtualFree(buffer, 0, MEM_RELEASE);
      }

      CloseHandle(chrome_dll);
    }
  }

  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

// Set did_run "dr" in omaha's client state for this product.
bool SetDidRunState(const wchar_t* guid, const wchar_t* value) {
  std::wstring key_path(google_update::kRegPathClientState);
  key_path.append(L"\\").append(guid);
  HKEY reg_key;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, key_path.c_str(), 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &reg_key, NULL) == ERROR_SUCCESS) {
    // Note that the length here must be in bytes and must account for the
    // terminating null char.
    ::RegSetValueExW(reg_key, google_update::kRegDidRunField, 0, REG_SZ,
                     reinterpret_cast<const BYTE *>(value),
                     (::lstrlenW(value) + 1) * sizeof(wchar_t));
    ::RegCloseKey(reg_key);
    return true;
  }
  return false;
}

bool RecordDidRun(const wchar_t* guid) {
  return SetDidRunState(guid, L"1");
}

bool ClearDidRun(const wchar_t* guid) {
  return SetDidRunState(guid, L"0");
}

}
//=============================================================================

MainDllLoader::MainDllLoader() : dll_(NULL) {
}

MainDllLoader::~MainDllLoader() {
#ifdef PURIFY
  // We should never unload the dll. There is only risk and no gain from
  // doing so. The singleton dtors have been already run by AtExitManager.
  ::FreeLibrary(dll_);
#endif
}

// Loading chrome is an interesting afair. First we try loading from the current
// directory to support run-what-you-compile and other development scenarios.
// If that fails then we look at the 'CHROME_VERSION' env variable to determine
// if we should stick with an older dll version even if a new one is available
// to support upgrade-in-place scenarios, and if that fails we finally we look
// at the registry which should point us to the latest version.
HMODULE MainDllLoader::Load(std::wstring* version, std::wstring* file) {
  std::wstring dir(GetExecutablePath());
  *file = dir;
  HMODULE dll = LoadChromeWithDirectory(file);
  if (dll)
    return dll;

  if (!EnvQueryStr(
          BrowserDistribution::GetDistribution()->GetEnvVersionKey().c_str(),
          version)) {
    std::wstring reg_path(GetRegistryPath());
    // Look into the registry to find the latest version.
    if (!GetVersion(dir.c_str(), reg_path.c_str(), version))
      return NULL;
  }

  *file = dir;
  file->append(*version).append(L"\\");
  return LoadChromeWithDirectory(file);
}

// Launching is a matter of loading the right dll, setting the CHROME_VERSION
// environment variable and just calling the entry point. Derived classes can
// add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          sandbox::SandboxInterfaceInfo* sbox_info) {
  std::wstring version;
  std::wstring file;
  dll_ = Load(&version, &file);
  if (!dll_)
    return ResultCodes::MISSING_DATA;

  ::SetEnvironmentVariableW(
      BrowserDistribution::GetDistribution()->GetEnvVersionKey().c_str(),
      version.c_str());

  InitCrashReporterWithDllPath(file);
  OnBeforeLaunch(version);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return ResultCodes::BAD_PROCESS_TYPE;

  int rc = entry_point(instance, sbox_info, ::GetCommandLineW());
  return OnBeforeExit(rc);
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  RelaunchChromeBrowserWithNewCommandLineIfNeededFunc relaunch_function =
      reinterpret_cast<RelaunchChromeBrowserWithNewCommandLineIfNeededFunc>(
          ::GetProcAddress(dll_,
                           "RelaunchChromeBrowserWithNewCommandLineIfNeeded"));
  if (!relaunch_function) {
    LOG(ERROR) << "Could not find exported function "
               << "RelaunchChromeBrowserWithNewCommandLineIfNeeded";
  } else {
    relaunch_function();
  }
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 public:
  virtual std::wstring GetRegistryPath() {
    std::wstring key(google_update::kRegPathClients);
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    key.append(L"\\").append(dist->GetAppGuid());
    return key;
  }

  virtual void OnBeforeLaunch(const std::wstring& version) {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    RecordDidRun(dist->GetAppGuid().c_str());
  }

  virtual int OnBeforeExit(int return_code) {
    // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
    // so we need to reset the did_run signal so omaha does not count
    // this run as active usage.
    if (ResultCodes::NORMAL_EXIT_CANCEL == return_code) {
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      ClearDidRun(dist->GetAppGuid().c_str());
    }
    return return_code;
  }
};

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 public:
  virtual std::wstring GetRegistryPath() {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    return dist->GetVersionKey();
  }
};

MainDllLoader* MakeMainDllLoader() {
#if defined(GOOGLE_CHROME_BUILD)
  return new ChromeDllLoader();
#else
  return new ChromiumDllLoader();
#endif
}

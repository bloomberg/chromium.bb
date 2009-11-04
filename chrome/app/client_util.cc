// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);

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

// Expects that |dir| has a trailing backslash. |dir| is modified so it
// contains the full path that was tried. Caller must check for the return
// value not being null to dermine if this path contains a valid dll.
HMODULE LoadChromeWithDirectory(std::wstring* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  dir->append(installer_util::kChromeDll);
  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

// record did_run "dr" in client state.
bool RecordDidRun(const wchar_t* guid) {
  std::wstring key_path(google_update::kRegPathClientState);
  key_path.append(L"\\").append(guid);
  HKEY reg_key;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, key_path.c_str(), 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &reg_key, NULL) == ERROR_SUCCESS) {
    const wchar_t kVal[] = L"1";
    ::RegSetValueExW(reg_key, google_update::kRegDidRunField, 0, REG_SZ,
                     reinterpret_cast<const BYTE *>(kVal), sizeof(kVal));
    ::RegCloseKey(reg_key);
    return true;
  }
  return false;
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

  if (!EnvQueryStr(google_update::kEnvProductVersionKey, version)) {
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

  ::SetEnvironmentVariableW(google_update::kEnvProductVersionKey,
                            version.c_str());

  InitCrashReporterWithDllPath(file);

  OnBeforeLaunch(version);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return ResultCodes::BAD_PROCESS_TYPE;

  return entry_point(instance, sbox_info, ::GetCommandLineW());
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 public:
  virtual std::wstring GetRegistryPath() {
    std::wstring key(google_update::kRegPathClients);
    key.append(L"\\").append(google_update::kChromeGuid);
    return key;
  }

  virtual void OnBeforeLaunch(const std::wstring& version) {
    RecordDidRun(google_update::kChromeGuid);
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

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Gets chrome version according to the load path. |exe_path| must be the
// backslash terminated directory of the current chrome.exe.
bool GetVersion(const wchar_t* exe_path, const wchar_t* key_path,
                std::wstring* version) {
  HKEY reg_root = InstallUtil::IsPerUserInstall(exe_path) ? HKEY_CURRENT_USER :
                                                            HKEY_LOCAL_MACHINE;
  bool success = false;

  base::win::RegKey key(reg_root, key_path, KEY_QUERY_VALUE);
  if (key.Valid()) {
    // If 'new_chrome.exe' is present it means chrome was auto-updated while
    // running. We need to consult the opv value so we can load the old dll.
    // TODO(cpu) : This is solving the same problem as the environment variable
    // so one of them will eventually be deprecated.
    std::wstring new_chrome_exe(exe_path);
    new_chrome_exe.append(installer::kChromeNewExe);
    if (::PathFileExistsW(new_chrome_exe.c_str()) &&
        key.ReadValue(google_update::kRegOldVersionField,
                      version) == ERROR_SUCCESS) {
      success = true;
    } else {
      success = (key.ReadValue(google_update::kRegVersionField,
                               version) == ERROR_SUCCESS);
    }
  }

  return success;
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
// value not being null to determine if this path contains a valid dll.
HMODULE LoadChromeWithDirectory(std::wstring* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
#ifdef _WIN64
  if ((cmd_line.GetSwitchValueASCII(switches::kProcessType) ==
      switches::kNaClBrokerProcess) ||
      (cmd_line.GetSwitchValueASCII(switches::kProcessType) ==
      switches::kNaClLoaderProcess)) {
    // Load the 64-bit DLL when running in a 64-bit process.
    dir->append(installer::kChromeNaCl64Dll);
  } else {
    // Only NaCl broker and loader can be launched as Win64 processes.
    NOTREACHED();
    return NULL;
  }
#else
  dir->append(installer::kChromeDll);
#endif

#ifdef NDEBUG
  // Experimental pre-reading optimization
  // The idea is to pre read significant portion of chrome.dll in advance
  // so that subsequent hard page faults are avoided.
  if (!cmd_line.HasSwitch(switches::kProcessType)) {
    // The kernel brings in 8 pages for the code section at a time and 4 pages
    // for other sections. We can skip over these pages to avoid a soft page
    // fault which may not occur during code execution. However skipping 4K at
    // a time still has better performance over 32K and 16K according to data.
    // TODO(ananta)
    // Investigate this and tune.
    const size_t kStepSize = 4 * 1024;

    DWORD pre_read_size = 0;
    DWORD pre_read_step_size = kStepSize;
    DWORD pre_read = 1;

    base::win::RegKey key(HKEY_CURRENT_USER, L"Software\\Google\\ChromeFrame",
                          KEY_QUERY_VALUE);
    if (key.Valid()) {
      key.ReadValueDW(L"PreReadSize", &pre_read_size);
      key.ReadValueDW(L"PreReadStepSize", &pre_read_step_size);
      key.ReadValueDW(L"PreRead", &pre_read);
      key.Close();
    }
    if (pre_read) {
      TRACE_EVENT_BEGIN("PreReadImage", 0, "");
      file_util::PreReadImage(dir->c_str(), pre_read_size, pre_read_step_size);
      TRACE_EVENT_END("PreReadImage", 0, "");
    }
  }
#endif  // NDEBUG

  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

// Set did_run "dr" to |value| in Google Update's client state key for the
// product associated with |dist|, creating the client state key if necessary.
bool SetDidRunState(BrowserDistribution* dist, const wchar_t* value) {
  DCHECK(dist);
  DCHECK(value);
  bool wrote_dr = false;
  std::wstring product_key_path(dist->GetStateKey());
  base::win::RegKey reg_key;
  if (reg_key.Create(HKEY_CURRENT_USER, product_key_path.c_str(),
                     KEY_SET_VALUE) == ERROR_SUCCESS) {
    if (reg_key.WriteValue(google_update::kRegDidRunField,
                           value) == ERROR_SUCCESS) {
      wrote_dr = true;
    }
  }
  return wrote_dr;
}


void RecordDidRun(const std::wstring& dll_path) {
  static const wchar_t kSet[] = L"1";
  BrowserDistribution* product_dist = BrowserDistribution::GetDistribution();
  SetDidRunState(product_dist, kSet);

  // If this is a multi-install, also write the did_run value under the
  // multi key.
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  if (InstallUtil::IsMultiInstall(product_dist, system_level)) {
    BrowserDistribution* multi_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
    SetDidRunState(multi_dist, kSet);
  }
}

void ClearDidRun(const std::wstring& dll_path) {
  static const wchar_t kCleared[] = L"0";
  BrowserDistribution* product_dist = BrowserDistribution::GetDistribution();
  SetDidRunState(product_dist, kCleared);

  // If this is a multi-install, also clear the did_run value under the
  // multi key.
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  if (InstallUtil::IsMultiInstall(product_dist, system_level)) {
    BrowserDistribution* multi_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
    SetDidRunState(multi_dist, kCleared);
  }
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

// Loading chrome is an interesting affair. First we try loading from the
// current directory to support run-what-you-compile and other development
// scenarios.
// If that fails then we look at the --chrome-version command line flag followed
// by the 'CHROME_VERSION' env variable to determine if we should stick with an
// older dll version even if a new one is available to support upgrade-in-place
// scenarios.
// If that fails then finally we look at the registry which should point us
// to the latest version. This is the expected path for the first chrome.exe
// browser instance in an installed build.
HMODULE MainDllLoader::Load(std::wstring* out_version, std::wstring* out_file) {
  std::wstring dir(GetExecutablePath());
  *out_file = dir;
  HMODULE dll = LoadChromeWithDirectory(out_file);
  if (dll)
    return dll;

  std::wstring version_string;
  scoped_ptr<Version> version;
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kChromeVersion)) {
    version_string = cmd_line.GetSwitchValueNative(switches::kChromeVersion);
    version.reset(Version::GetVersionFromString(WideToASCII(version_string)));

    if (!version.get()) {
      // If a bogus command line flag was given, then abort.
      LOG(ERROR) << "Invalid version string received on command line: "
                 << version_string;
      return NULL;
    }
  }

  if (!version.get()) {
    if (EnvQueryStr(ASCIIToWide(chrome::kChromeVersionEnvVar).c_str(),
                    &version_string)) {
      version.reset(Version::GetVersionFromString(WideToASCII(version_string)));
    }
  }

  if (!version.get()) {
    std::wstring reg_path(GetRegistryPath());
    // Look into the registry to find the latest version. We don't validate
    // this by building a Version object to avoid harming normal case startup
    // time.
    version_string.clear();
    GetVersion(dir.c_str(), reg_path.c_str(), &version_string);
  }

  if (version.get() || !version_string.empty()) {
    *out_file = dir;
    *out_version = version_string;
    out_file->append(*out_version).append(L"\\");
    return LoadChromeWithDirectory(out_file);
  } else {
    return NULL;
  }
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

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(chrome::kChromeVersionEnvVar, WideToUTF8(version));

  InitCrashReporterWithDllPath(file);
  OnBeforeLaunch(file);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return ResultCodes::BAD_PROCESS_TYPE;

  int rc = entry_point(instance, sbox_info, ::GetCommandLineW());
  return OnBeforeExit(rc, file);
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

  virtual void OnBeforeLaunch(const std::wstring& dll_path) {
    RecordDidRun(dll_path);
  }

  virtual int OnBeforeExit(int return_code, const std::wstring& dll_path) {
    // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
    // so we need to reset the did_run signal so omaha does not count
    // this run as active usage.
    if (ResultCodes::NORMAL_EXIT_CANCEL == return_code) {
      ClearDidRun(dll_path);
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

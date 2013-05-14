// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Gets chrome version according to the load path. |exe_path| must be the
// backslash terminated directory of the current chrome.exe.
bool GetChromeVersion(const wchar_t* exe_dir, const wchar_t* key_path,
                      string16* version) {
  HKEY reg_root = InstallUtil::IsPerUserInstall(exe_dir) ? HKEY_CURRENT_USER :
                                                           HKEY_LOCAL_MACHINE;
  bool success = false;

  base::win::RegKey key(reg_root, key_path, KEY_QUERY_VALUE);
  if (key.Valid()) {
    // If 'new_chrome.exe' is present it means chrome was auto-updated while
    // running. We need to consult the opv value so we can load the old dll.
    // TODO(cpu) : This is solving the same problem as the environment variable
    // so one of them will eventually be deprecated.
    string16 new_chrome_exe(exe_dir);
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

// Not generic, we only handle strings up to 128 chars.
bool EnvQueryStr(const wchar_t* key_name, string16* value) {
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
HMODULE LoadChromeWithDirectory(string16* dir) {
  ::SetCurrentDirectoryW(dir->c_str());
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  dir->append(installer::kChromeDll);

  return ::LoadLibraryExW(dir->c_str(), NULL,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

void RecordDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(true, system_level);
}

void ClearDidRun(const string16& dll_path) {
  bool system_level = !InstallUtil::IsPerUserInstall(dll_path.c_str());
  GoogleUpdateSettings::UpdateDidRunState(false, system_level);
}

}  // namespace

string16 GetExecutablePath() {
  wchar_t path[MAX_PATH];
  ::GetModuleFileNameW(NULL, path, MAX_PATH);
  if (!::PathRemoveFileSpecW(path))
    return string16();
  string16 exe_path(path);
  return exe_path.append(1, L'\\');
}

//=============================================================================

MainDllLoader::MainDllLoader() : dll_(NULL) {
}

MainDllLoader::~MainDllLoader() {
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
HMODULE MainDllLoader::Load(string16* out_version, string16* out_file) {
  const string16 dir(GetExecutablePath());
  *out_file = dir;
  HMODULE dll = LoadChromeWithDirectory(out_file);
  if (dll)
    return dll;

  string16 version_string;
  Version version;
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kChromeVersion)) {
    version_string = cmd_line.GetSwitchValueNative(switches::kChromeVersion);
    version = Version(WideToASCII(version_string));

    if (!version.IsValid()) {
      // If a bogus command line flag was given, then abort.
      LOG(ERROR) << "Invalid command line version: " << version_string;
      return NULL;
    }
  }

  // If no version on the command line, then look at the version resource in
  // the current module and try loading that.
  if (!version.IsValid()) {
    scoped_ptr<FileVersionInfo> file_version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    if (file_version_info.get()) {
      version_string = file_version_info->file_version();
      version = Version(WideToASCII(version_string));
    }
  }

  // TODO(robertshield): in theory, these next two checks (env and registry)
  // should never be needed. Remove them when I become 100% certain this is
  // also true in practice.

  // If no version in the current module, then look in the environment.
  if (!version.IsValid()) {
    if (EnvQueryStr(ASCIIToWide(chrome::kChromeVersionEnvVar).c_str(),
                    &version_string)) {
      version = Version(WideToASCII(version_string));
      LOG_IF(ERROR, !version.IsValid()) << "Invalid environment version: "
                                        << version_string;
    }
  }

  // If no version in the environment, then look in the registry.
  if (!version.IsValid()) {
    version_string = GetVersion();
    if (version_string.empty()) {
      LOG(ERROR) << "Could not get Chrome DLL version.";
      return NULL;
    }
  }

  *out_file = dir;
  *out_version = version_string;
  out_file->append(*out_version).append(1, L'\\');
  dll = LoadChromeWithDirectory(out_file);
  if (!dll) {
    LOG(ERROR) << "Failed to load Chrome DLL from " << *out_file;
    return NULL;
  }

  return dll;
}

// Launching is a matter of loading the right dll, setting the CHROME_VERSION
// environment variable and just calling the entry point. Derived classes can
// add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          sandbox::SandboxInterfaceInfo* sbox_info) {
  string16 version;
  string16 file;
  dll_ = Load(&version, &file);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(chrome::kChromeVersionEnvVar, WideToUTF8(version));
  // TODO(erikwright): Remove this when http://crbug.com/174953 is fixed and
  // widely deployed.
  env->UnSetVar(env_vars::kGoogleUpdateIsMachineEnvVar);

  InitCrashReporter();
  OnBeforeLaunch(file);

  DLL_MAIN entry_point =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  if (!entry_point)
    return chrome::RESULT_CODE_BAD_PROCESS_TYPE;

  int rc = entry_point(instance, sbox_info);
  return OnBeforeExit(rc, file);
}

string16 MainDllLoader::GetVersion() {
  string16 reg_path(GetRegistryPath());
  string16 version_string;
  string16 dir(GetExecutablePath());
  if (!GetChromeVersion(dir.c_str(), reg_path.c_str(), &version_string))
    return string16();
  return version_string;
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
  virtual string16 GetRegistryPath() {
    string16 key(google_update::kRegPathClients);
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    key.append(L"\\").append(dist->GetAppGuid());
    return key;
  }

  virtual void OnBeforeLaunch(const string16& dll_path) {
    RecordDidRun(dll_path);
  }

  virtual int OnBeforeExit(int return_code, const string16& dll_path) {
    // NORMAL_EXIT_CANCEL is used for experiments when the user cancels
    // so we need to reset the did_run signal so omaha does not count
    // this run as active usage.
    if (chrome::RESULT_CODE_NORMAL_EXIT_CANCEL == return_code) {
      ClearDidRun(dll_path);
    }
    return return_code;
  }
};

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 public:
  virtual string16 GetRegistryPath() {
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

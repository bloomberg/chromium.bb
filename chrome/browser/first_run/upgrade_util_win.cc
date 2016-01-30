// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "ui/base/ui_base_switches.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "google_update/google_update_idl.h"
#endif

namespace {

bool GetNewerChromeFile(base::FilePath* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  *path = path->Append(installer::kChromeNewExe);
  return true;
}

bool InvokeGoogleUpdateForRename() {
#if defined(GOOGLE_CHROME_BUILD)
  base::win::ScopedComPtr<IProcessLauncher> ipl;
  if (!FAILED(ipl.CreateInstance(__uuidof(ProcessLauncherClass)))) {
    ULONG_PTR phandle = NULL;
    DWORD id = GetCurrentProcessId();
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    if (!FAILED(ipl->LaunchCmdElevated(dist->GetAppGuid().c_str(),
                                       google_update::kRegRenameCmdField,
                                       id,
                                       &phandle))) {
      HANDLE handle = HANDLE(phandle);
      WaitForSingleObject(handle, INFINITE);
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer::RENAME_SUCCESSFUL)
        return true;
    }
  }
#endif  // GOOGLE_CHROME_BUILD
  return false;
}

}  // namespace

namespace upgrade_util {

const char kRelaunchModeMetro[] = "relaunch.mode.metro";
const char kRelaunchModeDesktop[] = "relaunch.mode.desktop";
const char kRelaunchModeDefault[] = "relaunch.mode.default";

// TODO(shrikant): Have a map/array to quickly map enum to strings.
std::string RelaunchModeEnumToString(const RelaunchMode relaunch_mode) {
  if (relaunch_mode == RELAUNCH_MODE_METRO)
    return kRelaunchModeMetro;

  if (relaunch_mode == RELAUNCH_MODE_DESKTOP)
    return kRelaunchModeDesktop;

  // For the purpose of code flow, even in case of wrong value we will
  // return default re-launch mode.
  return kRelaunchModeDefault;
}

RelaunchMode RelaunchModeStringToEnum(const std::string& relaunch_mode) {
  if (relaunch_mode == kRelaunchModeMetro)
    return RELAUNCH_MODE_METRO;

  if (relaunch_mode == kRelaunchModeDesktop)
    return RELAUNCH_MODE_DESKTOP;

  return RELAUNCH_MODE_DEFAULT;
}

bool RelaunchChromeHelper(const base::CommandLine& command_line,
                          const RelaunchMode& relaunch_mode) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string version_str;

  // Get the version variable and remove it from the environment.
  if (env->GetVar(chrome::kChromeVersionEnvVar, &version_str))
    env->UnSetVar(chrome::kChromeVersionEnvVar);
  else
    version_str.clear();

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  // Explicitly make sure to relaunch chrome.exe rather than old_chrome.exe.
  // This can happen when old_chrome.exe is launched by a user.
  base::CommandLine chrome_exe_command_line = command_line;
  chrome_exe_command_line.SetProgram(
      chrome_exe.DirName().Append(installer::kChromeExe));

  return base::LaunchProcess(chrome_exe_command_line, base::LaunchOptions())
      .IsValid();
}

bool RelaunchChromeBrowser(const base::CommandLine& command_line) {
  return RelaunchChromeHelper(command_line, RELAUNCH_MODE_DEFAULT);
}

bool RelaunchChromeWithMode(const base::CommandLine& command_line,
                            const RelaunchMode& relaunch_mode) {
  return RelaunchChromeHelper(command_line, relaunch_mode);
}

bool IsUpdatePendingRestart() {
  base::FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  return base::PathExists(new_chrome_exe);
}

bool SwapNewChromeExeIfPresent() {
  base::FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  if (!base::PathExists(new_chrome_exe))
    return false;
  base::FilePath cur_chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &cur_chrome_exe))
    return false;

  // Open up the registry key containing current version and rename information.
  bool user_install = InstallUtil::IsPerUserInstall(cur_chrome_exe);
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  base::win::RegKey key;
  if (key.Open(reg_root, dist->GetVersionKey().c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    // First try to rename exe by launching rename command ourselves.
    std::wstring rename_cmd;
    if (key.ReadValue(google_update::kRegRenameCmdField,
                      &rename_cmd) == ERROR_SUCCESS) {
      base::LaunchOptions options;
      options.wait = true;
      options.start_hidden = true;
      base::Process process = base::LaunchProcess(rename_cmd, options);
      if (process.IsValid()) {
        DWORD exit_code;
        ::GetExitCodeProcess(process.Handle(), &exit_code);
        if (exit_code == installer::RENAME_SUCCESSFUL)
          return true;
      }
    }
  }

  // Rename didn't work so try to rename by calling Google Update
  return InvokeGoogleUpdateForRename();
}

bool IsRunningOldChrome() {
  // This figures out the actual file name that the section containing the
  // mapped exe refers to. This is used instead of GetModuleFileName because the
  // .exe may have been renamed out from under us while we've been running which
  // GetModuleFileName won't notice.
  wchar_t mapped_file_name[MAX_PATH * 2] = {};

  if (!::GetMappedFileName(::GetCurrentProcess(),
                           reinterpret_cast<void*>(::GetModuleHandle(NULL)),
                           mapped_file_name,
                           arraysize(mapped_file_name))) {
    return false;
  }

  base::FilePath file_name(base::FilePath(mapped_file_name).BaseName());
  return base::FilePath::CompareEqualIgnoreCase(file_name.value(),
                                                installer::kChromeOldExe);
}

bool DoUpgradeTasks(const base::CommandLine& command_line) {
  // The DelegateExecute verb handler finalizes pending in-use updates for
  // metro mode launches, as Chrome cannot be gracefully relaunched when
  // running in this mode.
  if (!SwapNewChromeExeIfPresent() && !IsRunningOldChrome())
    return false;
  // At this point the chrome.exe has been swapped with the new one.
  if (!RelaunchChromeBrowser(command_line)) {
    // The re-launch fails. Feel free to panic now.
    NOTREACHED();
  }
  return true;
}

}  // namespace upgrade_util

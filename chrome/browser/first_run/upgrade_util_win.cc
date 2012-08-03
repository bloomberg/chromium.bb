// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/win/metro.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update/google_update_idl.h"

namespace {

bool GetNewerChromeFile(FilePath* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  *path = path->Append(installer::kChromeNewExe);
  return true;
}

bool InvokeGoogleUpdateForRename() {
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
  return false;
}

FilePath GetMetroRelauncherPath(const FilePath& chrome_exe,
                                const std::string version_str) {
  FilePath path(chrome_exe.DirName());

  // The relauncher is ordinarily in the version directory.  When running in a
  // build tree however (where CHROME_VERSION is not set in the environment)
  // look for it in Chrome's directory.
  if (!version_str.empty())
    path = path.AppendASCII(version_str);

  return path.Append(installer::kDelegateExecuteExe);
}

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowser(const CommandLine& command_line) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string version_str;

  // Get the version variable and remove it from the environment.
  if (env->GetVar(chrome::kChromeVersionEnvVar, &version_str))
    env->UnSetVar(chrome::kChromeVersionEnvVar);
  else
    version_str.clear();

  if (!base::win::IsMetroProcess())
    return base::LaunchProcess(command_line, base::LaunchOptions(), NULL);

  // Pass this Chrome's Start Menu shortcut path and handle to the relauncher.
  // The relauncher will wait for this Chrome to exit then reactivate it by
  // way of the Start Menu shortcut.
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  // Get a handle to this process that can be passed to the relauncher.
  HANDLE current_process = GetCurrentProcess();
  base::win::ScopedHandle process_handle;
  if (!::DuplicateHandle(current_process, current_process, current_process,
                         process_handle.Receive(), SYNCHRONIZE, TRUE, 0)) {
    DPCHECK(false);
    return false;
  }

  // Make the command line for the relauncher.
  CommandLine relaunch_cmd(GetMetroRelauncherPath(chrome_exe, version_str));
  relaunch_cmd.AppendSwitchPath(switches::kRelaunchShortcut,
      ShellIntegration::GetStartMenuShortcut(chrome_exe));
  relaunch_cmd.AppendSwitchNative(switches::kWaitForHandle,
      base::UintToString16(reinterpret_cast<uint32>(process_handle.Get())));

  base::LaunchOptions launch_options;
  launch_options.inherit_handles = true;
  if (!base::LaunchProcess(relaunch_cmd, launch_options, NULL)) {
    DPLOG(ERROR) << "Failed to launch relauncher \""
                 << relaunch_cmd.GetCommandLineString() << "\"";
    return false;
  }

  return true;
}

bool IsUpdatePendingRestart() {
  FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  return file_util::PathExists(new_chrome_exe);
}

bool SwapNewChromeExeIfPresent() {
  FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  if (!file_util::PathExists(new_chrome_exe))
    return false;
  FilePath cur_chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &cur_chrome_exe))
    return false;

  // Open up the registry key containing current version and rename information.
  bool user_install =
      InstallUtil::IsPerUserInstall(cur_chrome_exe.value().c_str());
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  base::win::RegKey key;
  if (key.Open(reg_root, dist->GetVersionKey().c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {

    // Having just ascertained that we can swap, now check that we should: if
    // we are given an explicit --chrome-version flag, don't rename unless the
    // specified version matches the "pv" value. In practice, this is used to
    // defer Chrome Frame updates until the current version of the Chrome Frame
    // DLL component is loaded.
    const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
    if (cmd_line.HasSwitch(switches::kChromeVersion)) {
      std::string version_string =
          cmd_line.GetSwitchValueASCII(switches::kChromeVersion);
      Version cmd_version(version_string);

      std::wstring pv_value;
      if (key.ReadValue(google_update::kRegVersionField,
                        &pv_value) == ERROR_SUCCESS) {
        Version pv_version(WideToASCII(pv_value));
        if (cmd_version.IsValid() && pv_version.IsValid() &&
            !cmd_version.Equals(pv_version)) {
          return false;
        }
      }
    }

    // First try to rename exe by launching rename command ourselves.
    std::wstring rename_cmd;
    if (key.ReadValue(google_update::kRegRenameCmdField,
                      &rename_cmd) == ERROR_SUCCESS) {
      base::ProcessHandle handle;
      base::LaunchOptions options;
      options.wait = true;
      options.start_hidden = true;
      if (base::LaunchProcess(rename_cmd, options, &handle)) {
        DWORD exit_code;
        ::GetExitCodeProcess(handle, &exit_code);
        ::CloseHandle(handle);
        if (exit_code == installer::RENAME_SUCCESSFUL)
          return true;
      }
    }
  }

  // Rename didn't work so try to rename by calling Google Update
  return InvokeGoogleUpdateForRename();
}

bool DoUpgradeTasks(const CommandLine& command_line) {
  // The DelegateExecute verb handler finalizes pending in-use updates for
  // metro mode launches, as Chrome cannot be gracefully relaunched when
  // running in this mode.
  if (base::win::IsMetroProcess())
    return false;
  if (!SwapNewChromeExeIfPresent())
    return false;
  // At this point the chrome.exe has been swapped with the new one.
  if (!RelaunchChromeBrowser(command_line)) {
    // The re-launch fails. Feel free to panic now.
    NOTREACHED();
  }
  return true;
}

}  // namespace upgrade_util

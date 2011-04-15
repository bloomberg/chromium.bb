// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update_idl.h"

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

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowser(const CommandLine& command_line) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->UnSetVar(chrome::kChromeVersionEnvVar);
  return base::LaunchApp(
      command_line.command_line_string(), false, false, NULL);
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

  // First try to rename exe by launching rename command ourselves.
  bool user_install =
      InstallUtil::IsPerUserInstall(cur_chrome_exe.value().c_str());
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  base::win::RegKey key;
  std::wstring rename_cmd;
  if ((key.Open(reg_root, dist->GetVersionKey().c_str(),
                KEY_READ) == ERROR_SUCCESS) &&
      (key.ReadValue(google_update::kRegRenameCmdField,
                     &rename_cmd) == ERROR_SUCCESS)) {
    base::ProcessHandle handle;
    if (base::LaunchApp(rename_cmd, true, true, &handle)) {
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer::RENAME_SUCCESSFUL)
        return true;
    }
  }

  // Rename didn't work so try to rename by calling Google Update
  return InvokeGoogleUpdateForRename();
}

bool DoUpgradeTasks(const CommandLine& command_line) {
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

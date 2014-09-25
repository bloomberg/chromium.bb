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
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/metro.h"
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
#include "google_update/google_update_idl.h"
#include "ui/base/ui_base_switches.h"

namespace {

bool GetNewerChromeFile(base::FilePath* path) {
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

base::FilePath GetMetroRelauncherPath(const base::FilePath& chrome_exe,
                                      const std::string& version_str) {
  base::FilePath path(chrome_exe.DirName());

  // The relauncher is ordinarily in the version directory.  When running in a
  // build tree however (where CHROME_VERSION is not set in the environment)
  // look for it in Chrome's directory.
  if (!version_str.empty())
    path = path.AppendASCII(version_str);

  return path.Append(installer::kDelegateExecuteExe);
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

  // On Windows 7 if the current browser is in Chrome OS mode, then restart
  // into Chrome OS mode.
  if ((base::win::GetVersion() == base::win::VERSION_WIN7) &&
       CommandLine::ForCurrentProcess()->HasSwitch(switches::kViewerConnect) &&
       g_browser_process->local_state()->HasPrefPath(prefs::kRelaunchMode)) {
    // TODO(ananta)
    // On Windows 8, the delegate execute process looks up the previously
    // launched mode from the registry and relaunches into that mode. We need
    // something similar on Windows 7. For now, set the pref to ensure that
    // we get relaunched into Chrome OS mode.
    g_browser_process->local_state()->SetString(
        prefs::kRelaunchMode, upgrade_util::kRelaunchModeMetro);
    return RELAUNCH_MODE_METRO;
  }

  return RELAUNCH_MODE_DEFAULT;
}

bool RelaunchChromeHelper(const CommandLine& command_line,
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
  CommandLine chrome_exe_command_line = command_line;
  chrome_exe_command_line.SetProgram(
      chrome_exe.DirName().Append(installer::kChromeExe));

  if (base::win::GetVersion() < base::win::VERSION_WIN8 &&
      relaunch_mode != RELAUNCH_MODE_METRO &&
      relaunch_mode != RELAUNCH_MODE_DESKTOP)
    return base::LaunchProcess(chrome_exe_command_line,
                               base::LaunchOptions(), NULL);

  // On Windows 8 we always use the delegate_execute for re-launching chrome.
  // On Windows 7 we use delegate_execute for re-launching chrome into Windows
  // ASH.
  //
  // Pass this Chrome's Start Menu shortcut path to the relauncher so it can re-
  // activate chrome via ShellExecute which will wait until we exit. Since
  // ShellExecute does not support handle passing to the child process we create
  // a uniquely named mutex that we aquire and never release. So when we exit,
  // Windows marks our mutex as abandoned and the wait is satisfied. The format
  // of the named mutex is important. See DelegateExecuteOperation for more
  // details.
  base::string16 mutex_name =
      base::StringPrintf(L"chrome.relaunch.%d", ::GetCurrentProcessId());
  HANDLE mutex = ::CreateMutexW(NULL, TRUE, mutex_name.c_str());
  // The |mutex| handle needs to be leaked. See comment above.
  if (!mutex) {
    NOTREACHED();
    return false;
  }
  if (::GetLastError() == ERROR_ALREADY_EXISTS) {
    NOTREACHED() << "Relaunch mutex already exists";
    return false;
  }

  CommandLine relaunch_cmd(CommandLine::NO_PROGRAM);
  relaunch_cmd.AppendSwitchPath(switches::kRelaunchShortcut,
      ShellIntegration::GetStartMenuShortcut(chrome_exe));
  relaunch_cmd.AppendSwitchNative(switches::kWaitForMutex, mutex_name);

  if (relaunch_mode != RELAUNCH_MODE_DEFAULT) {
    relaunch_cmd.AppendSwitch(relaunch_mode == RELAUNCH_MODE_METRO?
        switches::kForceImmersive : switches::kForceDesktop);
  }

  base::string16 params(relaunch_cmd.GetCommandLineString());
  base::string16 path(GetMetroRelauncherPath(chrome_exe, version_str).value());

  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_NOCLOSEPROCESS;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpFile = path.c_str();
  sei.lpParameters = params.c_str();

  if (!::ShellExecuteExW(&sei)) {
    NOTREACHED() << "ShellExecute failed with " << GetLastError();
    return false;
  }
  DWORD pid = ::GetProcessId(sei.hProcess);
  CloseHandle(sei.hProcess);
  if (!pid)
    return false;
  // The next call appears to be needed if we are relaunching from desktop into
  // metro mode. The observed effect if not done is that chrome starts in metro
  // mode but it is not given focus and it gets killed by windows after a few
  // seconds.
  ::AllowSetForegroundWindow(pid);
  return true;
}

bool RelaunchChromeBrowser(const CommandLine& command_line) {
  return RelaunchChromeHelper(command_line, RELAUNCH_MODE_DEFAULT);
}

bool RelaunchChromeWithMode(const CommandLine& command_line,
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
  bool user_install =
      InstallUtil::IsPerUserInstall(cur_chrome_exe.value().c_str());
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  base::win::RegKey key;
  if (key.Open(reg_root, dist->GetVersionKey().c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    // First try to rename exe by launching rename command ourselves.
    std::wstring rename_cmd;
    if (key.ReadValue(google_update::kRegRenameCmdField,
                      &rename_cmd) == ERROR_SUCCESS) {
      base::win::ScopedHandle handle;
      base::LaunchOptions options;
      options.wait = true;
      options.start_hidden = true;
      if (base::LaunchProcess(rename_cmd, options, &handle)) {
        DWORD exit_code;
        ::GetExitCodeProcess(handle.Get(), &exit_code);
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

bool DoUpgradeTasks(const CommandLine& command_line) {
  // The DelegateExecute verb handler finalizes pending in-use updates for
  // metro mode launches, as Chrome cannot be gracefully relaunched when
  // running in this mode.
  if (base::win::IsMetroProcess())
    return false;
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

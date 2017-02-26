// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;

namespace google_update {

namespace {

const int kGoogleUpdateTimeoutMs = 20 * 1000;

// Returns true if Google Update is present at the given level.
bool IsGoogleUpdatePresent(bool system_install) {
  // Using the existence of version key in the registry to decide.
  return GoogleUpdateSettings::GetGoogleUpdateVersion(system_install).IsValid();
}

// Returns GoogleUpdateSetup.exe's executable path at specified level.
// or an empty path if none is found.
base::FilePath GetGoogleUpdateSetupExe(bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key, kRegPathGoogleUpdate, KEY_QUERY_VALUE) ==
          ERROR_SUCCESS) {
    base::string16 path_str;
    base::string16 version_str;
    if ((update_key.ReadValue(kRegPathField, &path_str) == ERROR_SUCCESS) &&
        (update_key.ReadValue(kRegGoogleUpdateVersion, &version_str) ==
             ERROR_SUCCESS)) {
      return base::FilePath(path_str).DirName().Append(version_str).
          Append(kGoogleUpdateSetupExe);
    }
  }
  return base::FilePath();
}

// If Google Update is present at system-level, sets |cmd_string| to the command
// line to install Google Update at user-level and returns true.
// Otherwise, clears |cmd_string| and returns false.
bool GetUserLevelGoogleUpdateInstallCommandLine(base::string16* cmd_string) {
  cmd_string->clear();
  base::FilePath google_update_setup(
      GetGoogleUpdateSetupExe(true));  // system-level.
  if (!google_update_setup.empty()) {
    base::CommandLine cmd(google_update_setup);
    // Appends "/install runtime=true&needsadmin=false /silent /nomitag".
    // NB: /nomitag needs to be at the end.
    // Constants are found in code.google.com/p/omaha/common/const_cmd_line.h.
    cmd.AppendArg("/install");
    // The "&" can be used in base::LaunchProcess() without quotation
    // (this is problematic only if run from command prompt).
    cmd.AppendArg("runtime=true&needsadmin=false");
    cmd.AppendArg("/silent");
    cmd.AppendArg("/nomitag");
    *cmd_string = cmd.GetCommandLineString();
  }
  return !cmd_string->empty();
}

// Launches command |cmd_string|, and waits for |timeout| milliseconds before
// timing out.  To wait indefinitely, one can set
// |timeout| to be base::TimeDelta::FromMilliseconds(INFINITE).
// Returns true if this executes successfully.
// Returns false if command execution fails to execute, or times out.
bool LaunchProcessAndWaitWithTimeout(const base::string16& cmd_string,
                                     base::TimeDelta timeout) {
  bool success = false;
  int exit_code = 0;
  VLOG(0) << "Launching: " << cmd_string;
  base::Process process =
      base::LaunchProcess(cmd_string, base::LaunchOptions());
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch (" << cmd_string << ")";
  } else if (!process.WaitForExitWithTimeout(timeout, &exit_code)) {
    // The GetExitCodeProcess failed or timed-out.
    LOG(ERROR) <<"Command (" << cmd_string << ") is taking more than "
               << timeout.InMilliseconds() << " milliseconds to complete.";
  } else if (exit_code != 0) {
    LOG(ERROR) << "Command (" << cmd_string << ") exited with code "
               << exit_code;
  } else {
    success = true;
  }
  return success;
}

}  // namespace

bool EnsureUserLevelGoogleUpdatePresent() {
  VLOG(0) << "Ensuring Google Update is present at user-level.";

  bool success = false;
  if (IsGoogleUpdatePresent(false)) {
    success = true;
  } else {
    base::string16 cmd_string;
    if (!GetUserLevelGoogleUpdateInstallCommandLine(&cmd_string)) {
      LOG(ERROR) << "Cannot find Google Update at system-level.";
      // Ideally we should return false. However, this case should not be
      // encountered by regular users, and developers (who often installs
      // Chrome without Google Update) may be unduly impeded by this case.
      // Therefore we return true.
      success = true;
    } else {
      success = LaunchProcessAndWaitWithTimeout(cmd_string,
          base::TimeDelta::FromMilliseconds(INFINITE));
    }
  }
  return success;
}

bool UninstallGoogleUpdate(bool system_install) {
  bool success = false;
  base::string16 cmd_string(
      GoogleUpdateSettings::GetUninstallCommandLine(system_install));
  if (cmd_string.empty()) {
    success = true;  // Nothing to; vacuous success.
  } else {
    success = LaunchProcessAndWaitWithTimeout(cmd_string,
        base::TimeDelta::FromMilliseconds(kGoogleUpdateTimeoutMs));
  }
  return success;
}

void ElevateIfNeededToReenableUpdates() {
  installer::ProductState product_state;
  const bool system_install = !InstallUtil::IsPerUserInstall();
  if (!product_state.Initialize(system_install))
    return;
  base::FilePath exe_path(product_state.GetSetupPath());
  if (exe_path.empty() || !base::PathExists(exe_path)) {
    LOG(ERROR) << "Could not find setup.exe to reenable updates.";
    return;
  }

  base::CommandLine cmd(exe_path);
  cmd.AppendSwitch(installer::switches::kReenableAutoupdates);
  InstallUtil::AppendModeSwitch(&cmd);
  if (system_install)
    cmd.AppendSwitch(installer::switches::kSystemLevel);
  if (product_state.uninstall_command().HasSwitch(
          installer::switches::kVerboseLogging)) {
    cmd.AppendSwitch(installer::switches::kVerboseLogging);
  }

  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;

  if (base::win::UserAccountControlIsEnabled())
    base::LaunchElevatedProcess(cmd, launch_options);
  else
    base::LaunchProcess(cmd, launch_options);
}

}  // namespace google_update

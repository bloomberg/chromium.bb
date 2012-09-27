// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_util.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"

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
FilePath GetGoogleUpdateSetupExe(bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key, kRegPathGoogleUpdate, KEY_QUERY_VALUE) ==
          ERROR_SUCCESS) {
    string16 path_str;
    string16 version_str;
    if ((update_key.ReadValue(kRegPathField, &path_str) == ERROR_SUCCESS) &&
        (update_key.ReadValue(kRegGoogleUpdateVersion, &version_str) ==
             ERROR_SUCCESS)) {
      return FilePath(path_str).DirName().Append(version_str).
          Append(kGoogleUpdateSetupExe);
    }
  }
  return FilePath();
}

// If Google Update is present at system-level, sets |cmd_string| to the command
// line to install Google Update at user-level and returns true.
// Otherwise, clears |cmd_string| and returns false.
bool GetUserLevelGoogleUpdateInstallCommandLine(string16* cmd_string) {
  cmd_string->clear();
  FilePath google_update_setup(GetGoogleUpdateSetupExe(true));  // system-level.
  if (!google_update_setup.empty()) {
    CommandLine cmd(google_update_setup);
    // Appends parameter "/install runtime=true&needsadmin=false /silent"
    // Constants are found in code.google.com/p/omaha/common/const_cmd_line.h.
    cmd.AppendArg("/install");
    // The "&" can be used in base::LaunchProcess() without quotation
    // (this is problematic only if run from command prompt).
    cmd.AppendArg("runtime=true&needsadmin=false");
    cmd.AppendArg("/silent");
    *cmd_string = cmd.GetCommandLineString();
  }
  return !cmd_string->empty();
}

// Launches command |cmd_string|, and waits for |timeout| milliseconds before
// timing out.  To wait indefinitely, one can set
// |timeout| to be base::TimeDelta::FromMilliseconds(INFINITE).
// Returns true if this executes successfully.
// Returns false if command execution fails to execute, or times out.
bool LaunchProcessAndWaitWithTimeout(const string16& cmd_string,
                                     base::TimeDelta timeout) {
  bool success = false;
  base::win::ScopedHandle process;
  int exit_code = 0;
  LOG(INFO) << "Launching: " << cmd_string;
  if (!base::LaunchProcess(cmd_string, base::LaunchOptions(),
                           process.Receive())) {
    PLOG(ERROR) << "Failed to launch (" << cmd_string << ")";
  } else if (!base::WaitForExitCodeWithTimeout(process, &exit_code, timeout)) {
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
  LOG(INFO) << "Ensuring Google Update is present at user-level.";

  bool success = false;
  if (IsGoogleUpdatePresent(false)) {
    success = true;
  } else {
    string16 cmd_string;
    if (!GetUserLevelGoogleUpdateInstallCommandLine(&cmd_string)) {
      LOG(ERROR) << "Cannot find Google Update at system-level.";
    } else {
      success = LaunchProcessAndWaitWithTimeout(cmd_string,
          base::TimeDelta::FromMilliseconds(INFINITE));
    }
  }
  return success;
}

bool UninstallGoogleUpdate(bool system_install) {
  bool success = false;
  string16 cmd_string(
      GoogleUpdateSettings::GetUninstallCommandLine(system_install));
  if (cmd_string.empty()) {
    success = true;  // Nothing to; vacuous success.
  } else {
    success = LaunchProcessAndWaitWithTimeout(cmd_string,
        base::TimeDelta::FromMilliseconds(kGoogleUpdateTimeoutMs));
  }
  return success;
}

}  // namespace google_update

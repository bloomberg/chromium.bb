// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_util.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_split.h"
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

const char kEnvVariableUntrustedData[] = "GoogleUpdateUntrustedData";
const int kEnvVariableUntrustedDataMaxLength = 4096;

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

bool IsNotPrintable(unsigned char c) {
  return c < 32 || c >= 127;
}

// Returns whether or not |s| consists of printable characters.
bool IsStringPrintable(const std::string& s) {
  return std::find_if(s.begin(), s.end(), IsNotPrintable) == s.end();
}

bool IsIllegalUntrustedDataKeyChar(unsigned char c) {
  return !(c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' ||
           c >= '0' && c <= '9' || c == '-' || c == '_' || c == '$');
}

// Returns true if |key| from untrusted data is valid.
bool IsUntrustedDataKeyValid(const std::string& key) {
  return std::find_if(key.begin(), key.end(), IsIllegalUntrustedDataKeyChar)
      == key.end();
}

// Reads and parses untrusted data passed from Google Update as key-value
// pairs, then overwrites |untrusted_data_map| with the result.
// Returns true if data are successfully read.
bool GetGoogleUpdateUntrustedData(
    std::map<std::string, std::string>* untrusted_data) {
  DCHECK(untrusted_data);
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string data_string;
  if (env == NULL || !env->GetVar(kEnvVariableUntrustedData, &data_string))
    return false;

  if (data_string.length() > kEnvVariableUntrustedDataMaxLength ||
      !IsStringPrintable(data_string)) {
    LOG(ERROR) << "Invalid value in " << kEnvVariableUntrustedData;
    return false;
  }

  VLOG(1) << kEnvVariableUntrustedData << ": " << data_string;

  std::vector<std::pair<std::string, std::string> > kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(data_string, '=', '&', &kv_pairs)) {
    LOG(ERROR) << "Failed to parse untrusted data: " << data_string;
    return false;
  }

  untrusted_data->clear();
  std::vector<std::pair<std::string, std::string> >::const_iterator it;
  for (it = kv_pairs.begin(); it != kv_pairs.end(); ++it) {
    const std::string& key(it->first);
    // TODO(huangs): URL unescape |value|.
    const std::string& value(it->second);
    if (IsUntrustedDataKeyValid(key) && IsStringPrintable(value))
      (*untrusted_data)[key] = value;
    else
      LOG(ERROR) << "Illegal character found in untrusted data.";
  }
  return true;
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

std::string GetUntrustedDataValue(const std::string& key) {
  std::map<std::string, std::string> untrusted_data;
  if (GetGoogleUpdateUntrustedData(&untrusted_data)) {
    std::map<std::string, std::string>::const_iterator data_it(
        untrusted_data.find(key));
    if (data_it != untrusted_data.end())
      return data_it->second;
  }

  return std::string();
}

}  // namespace google_update

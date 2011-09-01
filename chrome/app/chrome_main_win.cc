// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_switches.h"
#include "policy/policy_constants.h"

namespace {

// Checks if the registry key exists in the given hive and expands any
// variables in the string.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive,
                                       const std::wstring& key_name,
                                       FilePath* user_data_dir) {
  std::wstring value;

  base::win::RegKey hklm_policy_key(hive, policy::kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *user_data_dir = FilePath(policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

}  // namespace

namespace chrome_main {

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  DCHECK(user_data_dir);
  // We are running as Chrome Frame if we were invoked with user-data-dir,
  // chrome-frame, and automation-channel switches.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const bool is_chrome_frame =
      !user_data_dir->empty() &&
      command_line->HasSwitch(switches::kChromeFrame) &&
      command_line->HasSwitch(switches::kAutomationClientChannelID);

  // In the case of Chrome Frame, the last path component of the user-data-dir
  // provided on the command line must be preserved since it is specific to
  // CF's host.
  FilePath cf_host_dir;
  if (is_chrome_frame)
    cf_host_dir = user_data_dir->BaseName();

  // Policy from the HKLM hive has precedence over HKCU so if we have one here
  // we don't have to try to load HKCU.
  const char* key_name_ascii = (is_chrome_frame ? policy::key::kGCFUserDataDir :
                                policy::key::kUserDataDir);
  std::wstring key_name(ASCIIToWide(key_name_ascii));
  if (LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, key_name,
                                        user_data_dir) ||
      LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, key_name,
                                        user_data_dir)) {
    // A Group Policy value was loaded.  Append the Chrome Frame host directory
    // if relevant.
    if (is_chrome_frame)
      *user_data_dir = user_data_dir->Append(cf_host_dir);
  }
}

}  // namespace chrome_main

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "policy/policy_constants.h"

namespace {

// Checks if the registry key exists in the given hive and expands any
// variables in the string.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive, FilePath* user_data_dir) {
  std::wstring key_name = UTF8ToWide(policy::key::kUserDataDir);
  std::wstring value;

  base::win::RegKey hklm_policy_key(hive, policy::kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *user_data_dir = FilePath::FromWStringHack(
        policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

}  // namespace

// Checks if the UserDataDir policy has been set and returns its value in the
// |user_data_dir| parameter. If no policy is set the parameter is not changed.
void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  // Policy from the HKLM hive has precedence over HKCU so if we have one here
  // we don't have to try to load HKCU.
  if (!LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, user_data_dir))
    LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, user_data_dir);
}

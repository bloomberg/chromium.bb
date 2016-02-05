// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <stddef.h>
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"
#include "policy/policy_constants.h"

namespace {

// Checks if the key exists in the given hive and expands any string variables.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive, base::FilePath* dir) {
  base::string16 value;
  base::string16 key_name(base::ASCIIToUTF16(policy::key::kUserDataDir));
  base::win::RegKey key(hive, policy::kRegistryChromePolicyKey, KEY_READ);
  if (key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *dir = base::FilePath(policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

const WCHAR* kMachineNamePolicyVarName = L"${machine_name}";
const WCHAR* kUserNamePolicyVarName = L"${user_name}";
const WCHAR* kWinDocumentsFolderVarName = L"${documents}";
const WCHAR* kWinLocalAppDataFolderVarName = L"${local_app_data}";
const WCHAR* kWinRoamingAppDataFolderVarName = L"${roaming_app_data}";
const WCHAR* kWinProfileFolderVarName = L"${profile}";
const WCHAR* kWinProgramDataFolderVarName = L"${global_app_data}";
const WCHAR* kWinProgramFilesFolderVarName = L"${program_files}";
const WCHAR* kWinWindowsFolderVarName = L"${windows}";
const WCHAR* kWinClientName = L"${client_name}";
const WCHAR* kWinSessionName = L"${session_name}";

struct WinFolderNamesToCSIDLMapping {
  const WCHAR* name;
  int id;
};

// Mapping from variable names to Windows CSIDL ids.
const WinFolderNamesToCSIDLMapping win_folder_mapping[] = {
    { kWinWindowsFolderVarName,        CSIDL_WINDOWS},
    { kWinProgramFilesFolderVarName,   CSIDL_PROGRAM_FILES},
    { kWinProgramDataFolderVarName,    CSIDL_COMMON_APPDATA},
    { kWinProfileFolderVarName,        CSIDL_PROFILE},
    { kWinLocalAppDataFolderVarName,   CSIDL_LOCAL_APPDATA},
    { kWinRoamingAppDataFolderVarName, CSIDL_APPDATA},
    { kWinDocumentsFolderVarName,      CSIDL_PERSONAL}
};

}  // namespace

namespace policy {

namespace path_parser {

// Replaces all variable occurances in the policy string with the respective
// system settings values.
base::FilePath::StringType ExpandPathVariables(
    const base::FilePath::StringType& untranslated_string) {
  base::FilePath::StringType result(untranslated_string);
  if (result.length() == 0)
    return result;
  // Sanitize quotes in case of any around the whole string.
  if (result.length() > 1 &&
      ((result.front() == L'"' && result.back() == L'"') ||
       (result.front() == L'\'' && result.back() == L'\''))) {
    // Strip first and last char which should be matching quotes now.
    result = result.substr(1, result.length() - 2);
  }
  // First translate all path variables we recognize.
  for (size_t i = 0; i < arraysize(win_folder_mapping); ++i) {
    size_t position = result.find(win_folder_mapping[i].name);
    if (position != std::wstring::npos) {
      WCHAR path[MAX_PATH];
      ::SHGetSpecialFolderPath(0, path, win_folder_mapping[i].id, false);
      std::wstring path_string(path);
      result.replace(position, wcslen(win_folder_mapping[i].name), path_string);
    }
  }
  // Next translate other windows specific variables.
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::wstring::npos) {
    DWORD return_length = 0;
    ::GetUserName(NULL, &return_length);
    if (return_length != 0) {
      scoped_ptr<WCHAR[]> username(new WCHAR[return_length]);
      ::GetUserName(username.get(), &return_length);
      std::wstring username_string(username.get());
      result.replace(position, wcslen(kUserNamePolicyVarName), username_string);
    }
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::wstring::npos) {
    DWORD return_length = 0;
    ::GetComputerNameEx(ComputerNamePhysicalDnsHostname, NULL, &return_length);
    if (return_length != 0) {
      scoped_ptr<WCHAR[]> machinename(new WCHAR[return_length]);
      ::GetComputerNameEx(ComputerNamePhysicalDnsHostname,
                          machinename.get(), &return_length);
      std::wstring machinename_string(machinename.get());
      result.replace(
          position, wcslen(kMachineNamePolicyVarName), machinename_string);
    }
  }
  position = result.find(kWinClientName);
  if (position != std::wstring::npos) {
    LPWSTR buffer = NULL;
    DWORD buffer_length = 0;
    if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                     WTSClientName,
                                     &buffer, &buffer_length)) {
      std::wstring clientname_string(buffer);
      result.replace(position, wcslen(kWinClientName), clientname_string);
      ::WTSFreeMemory(buffer);
    }
  }
  position = result.find(kWinSessionName);
  if (position != std::wstring::npos) {
    LPWSTR buffer = NULL;
    DWORD buffer_length = 0;
    if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                     WTSWinStationName,
                                     &buffer, &buffer_length)) {
      std::wstring sessionname_string(buffer);
      result.replace(position, wcslen(kWinSessionName), sessionname_string);
      ::WTSFreeMemory(buffer);
    }
  }
  // TODO(pastarmovj): Consider reorganizing this code once there are even more
  // variables to be supported. The search for the var and its replacement can
  // be extracted as common functionality.

  return result;
}

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  DCHECK(user_data_dir);
  // Policy from the HKLM hive has precedence over HKCU.
  if (!LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, user_data_dir))
    LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, user_data_dir);
}

}  // namespace path_parser

}  // namespace policy

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"
#include "policy/policy_constants.h"

namespace {

// Checks if the key exists in the given hive and expands any string variables.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive, base::FilePath* dir) {
  std::wstring value;
  std::wstring key_name(base::ASCIIToWide(policy::key::kUserDataDir));
  base::win::RegKey key(hive, policy::kRegistryChromePolicyKey, KEY_READ);
  if (key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *dir = base::FilePath(policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

}  // namespace

namespace policy {

namespace path_parser {

namespace internal {

bool GetSpecialFolderPath(int id, base::FilePath::StringType* value) {
  WCHAR path[MAX_PATH];
  ::SHGetSpecialFolderPath(0, path, id, false);
  *value = base::FilePath::StringType(path);
  return true;
}

bool GetWindowsUserName(base::FilePath::StringType* value) {
  DWORD return_length = 0;
  ::GetUserName(NULL, &return_length);
  if (return_length) {
    scoped_ptr<WCHAR[]> username(new WCHAR[return_length]);
    ::GetUserName(username.get(), &return_length);
    *value = base::FilePath::StringType(username.get());
  }
  return (return_length != 0);
}

bool GetMachineName(base::FilePath::StringType* value) {
  DWORD return_length = 0;
  ::GetComputerNameEx(ComputerNamePhysicalDnsHostname, NULL, &return_length);
  if (return_length) {
    scoped_ptr<WCHAR[]> machinename(new WCHAR[return_length]);
    ::GetComputerNameEx(
        ComputerNamePhysicalDnsHostname, machinename.get(), &return_length);
    *value = base::FilePath::StringType(machinename.get());
  }
  return (return_length != 0);
}

bool GetClientName(base::FilePath::StringType* value) {
  LPWSTR buffer = NULL;
  DWORD buffer_length = 0;
  BOOL status;
  if ((status = ::WTSQuerySessionInformation(WTS_CURRENT_SERVER,
                                             WTS_CURRENT_SESSION,
                                             WTSClientName,
                                             &buffer,
                                             &buffer_length))) {
    *value = base::FilePath::StringType(buffer);
    ::WTSFreeMemory(buffer);
  }
  return (status == TRUE);
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

// A table mapping variable name to their respective getvalue callbacks.
const VariableNameAndValueCallback kVariableNameAndValueCallbacks[] = {
    VariableNameAndValueCallback(
        kWinWindowsFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_WINDOWS)),
    VariableNameAndValueCallback(
        kWinProgramFilesFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_PROGRAM_FILES)),
    VariableNameAndValueCallback(
        kWinProgramDataFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_COMMON_APPDATA)),
    VariableNameAndValueCallback(
        kWinProfileFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_PROFILE)),
    VariableNameAndValueCallback(
        kWinLocalAppDataFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_LOCAL_APPDATA)),
    VariableNameAndValueCallback(
        kWinRoamingAppDataFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_APPDATA)),
    VariableNameAndValueCallback(
        kWinDocumentsFolderVarName,
        base::Bind(GetSpecialFolderPath, CSIDL_PERSONAL)),
    VariableNameAndValueCallback(kUserNamePolicyVarName,
                                 base::Bind(GetWindowsUserName)),
    VariableNameAndValueCallback(kMachineNamePolicyVarName,
                                 base::Bind(GetMachineName)),
    VariableNameAndValueCallback(kWinClientName, base::Bind(GetClientName))};

// Total number of entries in the mapping table.
const int kNoOfVariables = arraysize(kVariableNameAndValueCallbacks);

}  // namespace internal

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  DCHECK(user_data_dir);
  // Policy from the HKLM hive has precedence over HKCU.
  if (!LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, user_data_dir))
    LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, user_data_dir);
}

}  // namespace path_parser

}  // namespace policy

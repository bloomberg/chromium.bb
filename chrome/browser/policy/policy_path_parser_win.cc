// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace policy {

namespace path_parser {

const WCHAR* kMachineNamePolicyVarName = L"${machine_name}";
const WCHAR* kUserNamePolicyVarName = L"${user_name}";
const WCHAR* kWinDocumentsFolderVarName = L"${documents}";
const WCHAR* kWinLocalAppDataFolderVarName = L"${local_app_data}";
const WCHAR* kWinProfileFolderVarName = L"${profile}";
const WCHAR* kWinProgramDataFolderVarName = L"${global_app_data}";
const WCHAR* kWinProgramFilesFolderVarName = L"${program_files}";
const WCHAR* kWinWindowsFolderVarName = L"${windows}";

struct WinFolderNamesToCSIDLMapping {
  const WCHAR* name;
  int id;
};

// Mapping from variable names to Windows CSIDL ids.
const WinFolderNamesToCSIDLMapping win_folder_mapping[] = {
    { kWinWindowsFolderVarName,      CSIDL_WINDOWS},
    { kWinProgramFilesFolderVarName, CSIDL_PROGRAM_FILES},
    { kWinProgramDataFolderVarName,  CSIDL_COMMON_APPDATA},
    { kWinProfileFolderVarName,      CSIDL_PROFILE},
    { kWinLocalAppDataFolderVarName, CSIDL_LOCAL_APPDATA},
    { kWinDocumentsFolderVarName,    CSIDL_PERSONAL}
};

// Replaces all variable occurances in the policy string with the respective
// system settings values.
FilePath::StringType ExpandPathVariables(
    const FilePath::StringType& untranslated_string) {
  FilePath::StringType result(untranslated_string);
  // First translate all path variables we recognize.
  for (int i = 0; i < arraysize(win_folder_mapping); ++i) {
    size_t position = result.find(win_folder_mapping[i].name);
    if (position != std::wstring::npos) {
      WCHAR path[MAX_PATH];
      ::SHGetSpecialFolderPath(0, path, win_folder_mapping[i].id, false);
      std::wstring path_string(path);
      result.replace(position, wcslen(win_folder_mapping[i].name), path_string);
    }
  }
  // Next translate two speacial variables ${user_name} and ${machine_name}
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::wstring::npos) {
    DWORD return_length = 0;
    ::GetUserName(NULL, &return_length);
    if (return_length != 0) {
      scoped_array<WCHAR> username(new WCHAR[return_length]);
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
      scoped_array<WCHAR> machinename(new WCHAR[return_length]);
      ::GetComputerNameEx(ComputerNamePhysicalDnsHostname,
                          machinename.get(), &return_length);
      std::wstring machinename_string(machinename.get());
      result.replace(
          position, wcslen(kMachineNamePolicyVarName), machinename_string);
    }
  }
  return result;
}

}  // namespace path_parser

}  // namespace policy

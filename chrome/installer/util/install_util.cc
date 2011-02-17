// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See the corresponding header file for description of the functions in this
// file.

#include "chrome/installer/util/install_util.h"

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;
using installer::ProductState;

bool InstallUtil::ExecuteExeAsAdmin(const CommandLine& cmd, DWORD* exit_code) {
  FilePath::StringType program(cmd.GetProgram().value());
  DCHECK(!program.empty());
  DCHECK_NE(program[0], L'\"');

  CommandLine::StringType params(cmd.command_line_string());
  if (params[0] == '"') {
    DCHECK_EQ('"', params[program.length() + 1]);
    DCHECK_EQ(program, params.substr(1, program.length()));
    params = params.substr(program.length() + 2);
  } else {
    DCHECK_EQ(program, params.substr(0, program.length()));
    params = params.substr(program.length());
  }

  TrimWhitespace(params, TRIM_ALL, &params);

  SHELLEXECUTEINFO info = {0};
  info.cbSize = sizeof(SHELLEXECUTEINFO);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = program.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOW;
  if (::ShellExecuteEx(&info) == FALSE)
    return false;

  ::WaitForSingleObject(info.hProcess, INFINITE);
  DWORD ret_val = 0;
  if (!::GetExitCodeProcess(info.hProcess, &ret_val))
    return false;

  if (exit_code)
    *exit_code = ret_val;
  return true;
}

CommandLine InstallUtil::GetChromeUninstallCmd(
    bool system_install, BrowserDistribution::Type distribution_type) {
  ProductState state;
  if (state.Initialize(system_install, distribution_type)) {
    return state.uninstall_command();
  }
  return CommandLine(CommandLine::NO_PROGRAM);
}

Version* InstallUtil::GetChromeVersion(BrowserDistribution* dist,
                                       bool system_install) {
  DCHECK(dist);
  RegKey key;
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LONG result = key.Open(reg_root, dist->GetVersionKey().c_str(), KEY_READ);

  std::wstring version_str;
  if (result == ERROR_SUCCESS)
    result = key.ReadValue(google_update::kRegVersionField, &version_str);

  Version* ret = NULL;
  if (result == ERROR_SUCCESS && !version_str.empty()) {
    VLOG(1) << "Existing " << dist->GetApplicationName()
            << " version found " << version_str;
    ret = Version::GetVersionFromString(WideToASCII(version_str));
  } else {
    DCHECK_EQ(ERROR_FILE_NOT_FOUND, result);
    VLOG(1) << "No existing " << dist->GetApplicationName()
            << " install found.";
  }

  return ret;
}

bool InstallUtil::IsOSSupported() {
  int major, minor;
  base::win::Version version = base::win::GetVersion();
  base::win::GetServicePackLevel(&major, &minor);

  // We do not support Win2K or older, or XP without service pack 2.
  VLOG(1) << "Windows Version: " << version
          << ", Service Pack: " << major << "." << minor;
  return (version > base::win::VERSION_XP) ||
      (version == base::win::VERSION_XP && major >= 2);
}

void InstallUtil::WriteInstallerResult(bool system_install,
                                       const std::wstring& state_key,
                                       installer::InstallStatus status,
                                       int string_resource_id,
                                       const std::wstring* const launch_cmd) {
  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  DWORD installer_result = (GetInstallReturnCode(status) == 0) ? 0 : 1;
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddCreateRegKeyWorkItem(root, state_key);
  install_list->AddSetRegValueWorkItem(root, state_key,
                                       installer::kInstallerResult,
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, state_key,
                                       installer::kInstallerError,
                                       static_cast<DWORD>(status), true);
  if (string_resource_id != 0) {
    std::wstring msg = installer::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, state_key,
        installer::kInstallerResultUIString, msg, true);
  }
  if (launch_cmd != NULL && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(root, state_key,
        installer::kInstallerSuccessLaunchCmdLine, *launch_cmd, true);
  }
  if (!install_list->Do())
    LOG(ERROR) << "Failed to record installer error information in registry.";
}

bool InstallUtil::IsPerUserInstall(const wchar_t* const exe_path) {
  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    return !StartsWith(exe_path, program_files_path, false);
  } else {
    NOTREACHED();
  }
  return true;
}

bool CheckIsChromeSxSProcess() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CHECK(command_line);

  if (command_line->HasSwitch(installer::switches::kChromeSxS))
    return true;

  // Also return true if we are running from Chrome SxS installed path.
  FilePath exe_dir;
  PathService::Get(base::DIR_EXE, &exe_dir);
  std::wstring chrome_sxs_dir(installer::kGoogleChromeInstallSubDir2);
  chrome_sxs_dir.append(installer::kSxSSuffix);
  return FilePath::CompareEqualIgnoreCase(exe_dir.BaseName().value(),
                                          installer::kInstallBinaryDir) &&
         FilePath::CompareEqualIgnoreCase(exe_dir.DirName().BaseName().value(),
                                          chrome_sxs_dir);
}

bool InstallUtil::IsChromeSxSProcess() {
  static bool sxs = CheckIsChromeSxSProcess();
  return sxs;
}

bool InstallUtil::BuildDLLRegistrationList(const std::wstring& install_path,
                                           const wchar_t** const dll_names,
                                           int dll_names_count,
                                           bool do_register,
                                           bool user_level_registration,
                                           WorkItemList* registration_list) {
  DCHECK(NULL != registration_list);
  bool success = true;
  for (int i = 0; i < dll_names_count; i++) {
    std::wstring dll_file_path(install_path);
    file_util::AppendToPath(&dll_file_path, dll_names[i]);
    success = registration_list->AddSelfRegWorkItem(dll_file_path,
        do_register, user_level_registration) && success;
  }
  return (dll_names_count > 0) && success;
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool InstallUtil::DeleteRegistryKey(RegKey& root_key,
                                    const std::wstring& key_path) {
  VLOG(1) << "Deleting registry key " << key_path;
  LONG result = root_key.DeleteKey(key_path.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    PLOG(ERROR) << "Failed to delete registry key: " << key_path
                << " error: " << result;
    return false;
  }
  return true;
}

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool InstallUtil::DeleteRegistryValue(HKEY reg_root,
                                      const std::wstring& key_path,
                                      const std::wstring& value_name) {
  RegKey key(reg_root, key_path.c_str(), KEY_ALL_ACCESS);
  VLOG(1) << "Deleting registry value " << value_name;
  if (key.ValueExists(value_name.c_str())) {
    LONG result = key.DeleteValue(value_name.c_str());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete registry value: " << value_name
                 << " error: " << result;
      return false;
    }
  }
  return true;
}

// static
int InstallUtil::GetInstallReturnCode(installer::InstallStatus status) {
  switch (status) {
    case installer::FIRST_INSTALL_SUCCESS:
    case installer::INSTALL_REPAIRED:
    case installer::NEW_VERSION_UPDATED:
    case installer::IN_USE_UPDATED:
      return 0;
    default:
      return status;
  }
}

// static
void InstallUtil::MakeUninstallCommand(const std::wstring& exe_path,
                                       const std::wstring& arguments,
                                       CommandLine* command_line) {
  const bool no_program = exe_path.empty();

  // Return a bunch of nothingness.
  if (no_program && arguments.empty()) {
    *command_line = CommandLine(CommandLine::NO_PROGRAM);
  } else {
    // Form a full command line string.
    std::wstring command;
    command.append(1, L'"')
        .append(no_program ? L"" : exe_path)
        .append(L"\" ")
        .append(arguments);

    // If we have a program name, return this complete command line.
    *command_line = CommandLine::FromString(command);
  }
}

std::wstring InstallUtil::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[arraysize(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, NULL, kDateFormat,
                           date_str, arraysize(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    PLOG(DFATAL) << "GetDateFormat";
  }

  return std::wstring(date_str, len);
}

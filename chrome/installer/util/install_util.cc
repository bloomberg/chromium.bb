// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/win_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

bool InstallUtil::ExecuteExeAsAdmin(const std::wstring& exe,
                                    const std::wstring& params,
                                    DWORD* exit_code) {
  SHELLEXECUTEINFO info = {0};
  info.cbSize = sizeof(SHELLEXECUTEINFO);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = exe.c_str();
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

std::wstring InstallUtil::GetChromeUninstallCmd(bool system_install) {
  HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(root, dist->GetUninstallRegPath().c_str(), KEY_READ);
  std::wstring uninstall_cmd;
  key.ReadValue(installer_util::kUninstallStringField, &uninstall_cmd);
  return uninstall_cmd;
}

installer::Version* InstallUtil::GetChromeVersion(bool system_install) {
  RegKey key;
  std::wstring version_str;

  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!key.Open(reg_root, dist->GetVersionKey().c_str(), KEY_READ) ||
      !key.ReadValue(google_update::kRegVersionField, &version_str)) {
    LOG(INFO) << "No existing Chrome install found.";
    key.Close();
    return NULL;
  }
  key.Close();
  LOG(INFO) << "Existing Chrome version found " << version_str;
  return installer::Version::GetVersionFromString(version_str);
}

bool InstallUtil::IsOSSupported() {
  int major, minor;
  win_util::WinVersion version = win_util::GetWinVersion();
  win_util::GetServicePackLevel(&major, &minor);

  // We do not support Win2K or older, or XP without service pack 2.
  LOG(INFO) << "Windows Version: " << version
            << ", Service Pack: " << major << "." << minor;
  if ((version > win_util::WINVERSION_XP) ||
      (version == win_util::WINVERSION_XP && major >= 2)) {
    return true;
  }
  return false;
}

void InstallUtil::WriteInstallerResult(bool system_install,
                                       installer_util::InstallStatus status,
                                       int string_resource_id,
                                       const std::wstring* const launch_cmd) {
  HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring key = dist->GetStateKey();
  int installer_result = (dist->GetInstallReturnCode(status) == 0) ? 0 : 1;
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddCreateRegKeyWorkItem(root, key);
  install_list->AddSetRegValueWorkItem(root, key, L"InstallerResult",
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, key, L"InstallerError",
                                       status, true);
  if (string_resource_id != 0) {
    std::wstring msg = installer_util::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, key, L"InstallerResultUIString",
                                         msg, true);
  }
  if (launch_cmd != NULL && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(root, key,
                                         L"InstallerSuccessLaunchCmdLine",
                                         *launch_cmd, true);
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

bool InstallUtil::IsChromeFrameProcess() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  DCHECK(command_line)
      << "IsChromeFrameProcess() called before ComamandLine::Init()";

  FilePath module_path;
  PathService::Get(base::FILE_MODULE, &module_path);
  std::wstring module_name(module_path.BaseName().value());

  scoped_ptr<DictionaryValue> prefs(installer_util::GetInstallPreferences(
                                    *command_line));
  DCHECK(prefs.get());
  bool is_cf = false;
  installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kChromeFrame, &is_cf);

  // Also assume this to be a ChromeFrame process if we are running inside
  // the Chrome Frame DLL.
  return is_cf || module_name == installer_util::kChromeFrameDll;
}

bool InstallUtil::IsChromeSxSProcess() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CHECK(command_line);

  if (command_line->HasSwitch(installer_util::switches::kChromeSxS))
    return true;

  // Also return true if we are running from Chrome SxS installed path.
  FilePath exe_dir;
  PathService::Get(base::DIR_EXE, &exe_dir);
  std::wstring chrome_sxs_dir(installer_util::kGoogleChromeInstallSubDir2);
  chrome_sxs_dir.append(installer_util::kSxSSuffix);
  return FilePath::CompareEqualIgnoreCase(exe_dir.BaseName().value(),
                                          installer_util::kInstallBinaryDir) &&
         FilePath::CompareEqualIgnoreCase(exe_dir.DirName().BaseName().value(),
                                          chrome_sxs_dir);
}

bool InstallUtil::IsMSIProcess(bool system_level) {
  // Initialize the static msi flags.
  static bool is_msi_ = false;
  static bool msi_checked_ = false;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CHECK(command_line);

  if (!msi_checked_) {
    msi_checked_ = true;

    scoped_ptr<DictionaryValue> prefs(installer_util::GetInstallPreferences(
                                      *command_line));
    DCHECK(prefs.get());
    bool is_msi = false;
    installer_util::GetDistroBooleanPreference(prefs.get(),
        installer_util::master_preferences::kMsi, &is_msi);

    if (!is_msi) {
      // We didn't find it in the preferences, try looking in the registry.
      HKEY reg_root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      RegKey key;
      DWORD msi_value;

      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      DCHECK(dist);

      bool success = false;
      std::wstring reg_key(dist->GetStateKey());
      if (key.Open(reg_root, reg_key.c_str(), KEY_READ | KEY_WRITE)) {
        if (key.ReadValueDW(google_update::kRegMSIField, &msi_value)) {
          is_msi = (msi_value == 1);
        }
      }
    }

    is_msi_ = is_msi;
  }

  return is_msi_;
}

bool InstallUtil::SetMSIMarker(bool system_level, bool set) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  DCHECK(dist);
  std::wstring client_state_path(dist->GetStateKey());

  bool success = false;
  HKEY reg_root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey client_state_key;
  if (client_state_key.Open(reg_root, client_state_path.c_str(),
                            KEY_READ | KEY_WRITE)) {
    DWORD msi_value = set ? 1 : 0;
    if (client_state_key.WriteValue(google_update::kRegMSIField, msi_value)) {
      success = true;
    } else {
      LOG(ERROR) << "Could not write msi value to client state key.";
    }
  } else {
    LOG(ERROR) << "Could not open client state key!";
  }

  return success;
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
  LOG(INFO) << "Deleting registry key " << key_path;
  if (!root_key.DeleteKey(key_path.c_str()) &&
      ::GetLastError() != ERROR_MOD_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path;
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
  LOG(INFO) << "Deleting registry value " << value_name;
  if (key.ValueExists(value_name.c_str()) &&
      !key.DeleteValue(value_name.c_str())) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name;
    return false;
  }
  return true;
}

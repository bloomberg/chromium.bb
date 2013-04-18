// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/common/win/install_utils.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/registry.h"

namespace cloud_print {

namespace {

// Google Update related constants.
const wchar_t kClientsKey[] = L"SOFTWARE\\Google\\Update\\Clients\\";
const wchar_t kClientStateKey[] = L"SOFTWARE\\Google\\Update\\ClientState\\";
const wchar_t* kUsageKey = L"dr";
const wchar_t kVersionKey[] = L"pv";
const wchar_t kNameKey[] = L"name";
const DWORD kInstallerResultFailedCustomError = 1;
const wchar_t kRegValueInstallerResult[] = L"InstallerResult";
const wchar_t kRegValueInstallerResultUIString[] = L"InstallerResultUIString";

// Uninstall related constants.
const wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
const wchar_t kInstallLocation[] = L"InstallLocation";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kDisplayName[] = L"DisplayName";
const wchar_t kPublisher[] = L"Publisher";
const wchar_t kNoModify[] = L"NoModify";
const wchar_t kNoRepair[] = L"NoRepair";

}  // namespace


void SetGoogleUpdateKeys(const string16& product_id,
                         const string16& product_name) {
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kClientsKey + product_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  // Get the version from the resource file.
  string16 version_string;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());

  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    version_string = version_info_win->product_version();
  } else {
    LOG(ERROR) << "Unable to get version string";
    // Use a random version string so that Google Update has something to go by.
    version_string = L"0.0.0.99";
  }

  if (key.WriteValue(kVersionKey, version_string.c_str()) != ERROR_SUCCESS ||
      key.WriteValue(kNameKey, product_name.c_str()) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set registry keys";
  }
}

void SetGoogleUpdateError(const string16& product_id, const string16& message) {
  LOG(ERROR) << message;
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kClientStateKey + product_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  if (key.WriteValue(kRegValueInstallerResult,
                     kInstallerResultFailedCustomError) != ERROR_SUCCESS ||
      key.WriteValue(kRegValueInstallerResultUIString,
                     message.c_str()) != ERROR_SUCCESS) {
      LOG(ERROR) << "Unable to set registry keys";
  }
}

void DeleteGoogleUpdateKeys(const string16& product_id) {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE,
               (cloud_print::kClientsKey + product_id).c_str(),
               DELETE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key to delete";
    return;
  }
  if (key.DeleteKey(L"") != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to delete key";
  }
}

void CreateUninstallKey(const string16& uninstall_id,
                        const string16& product_name,
                        const std::string& uninstall_switch) {
  // Now write the Windows Uninstall entries
  // Minimal error checking here since the install can continue
  // if this fails.
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kUninstallKey + uninstall_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
    return;
  }

  base::FilePath unstall_binary;
  CHECK(PathService::Get(base::FILE_EXE, &unstall_binary));

  CommandLine uninstall_command(unstall_binary);
  uninstall_command.AppendSwitch(uninstall_switch);
  key.WriteValue(kUninstallKey,
                 uninstall_command.GetCommandLineString().c_str());
  key.WriteValue(kInstallLocation,
                 unstall_binary.DirName().value().c_str());

  // Get the version resource.
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());

  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    key.WriteValue(kDisplayVersion,
                   version_info_win->file_version().c_str());
    key.WriteValue(kPublisher, version_info_win->company_name().c_str());
  } else {
    LOG(ERROR) << "Unable to get version string";
  }
  key.WriteValue(kDisplayName, product_name.c_str());
  key.WriteValue(kNoModify, 1);
  key.WriteValue(kNoRepair, 1);
}

void DeleteUninstallKey(const string16& uninstall_id) {
  ::RegDeleteKey(HKEY_LOCAL_MACHINE,
                 (cloud_print::kUninstallKey + uninstall_id).c_str());
}

base::FilePath GetInstallLocation(const string16& uninstall_id) {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE,
               (cloud_print::kUninstallKey + uninstall_id).c_str(),
               KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    // Not installed.
    return base::FilePath();
  }
  string16 install_path_value;
  key.ReadValue(kInstallLocation, &install_path_value);
  return base::FilePath(install_path_value);
}

}  // namespace cloud_print


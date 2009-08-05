// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"


DictionaryValue* setup_util::GetInstallPreferences(
    const CommandLine& cmd_line) {
  DictionaryValue* prefs = NULL;

  if (cmd_line.HasSwitch(installer_util::switches::kInstallerData)) {
    FilePath prefs_path(
        cmd_line.GetSwitchValue(installer_util::switches::kInstallerData));
    prefs = installer_util::ParseDistributionPreferences(prefs_path);
  }

  if (!prefs)
    prefs = new DictionaryValue();

  if (cmd_line.HasSwitch(installer_util::switches::kCreateAllShortcuts))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kCreateAllShortcuts, true);

  if (cmd_line.HasSwitch(installer_util::switches::kDoNotLaunchChrome))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kDoNotLaunchChrome, true);

  if (cmd_line.HasSwitch(installer_util::switches::kMakeChromeDefault))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kMakeChromeDefault, true);

  if (cmd_line.HasSwitch(installer_util::switches::kSystemLevel))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kSystemLevel, true);

  if (cmd_line.HasSwitch(installer_util::switches::kVerboseLogging))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kVerboseLogging, true);

  if (cmd_line.HasSwitch(installer_util::switches::kAltDesktopShortcut))
    installer_util::SetDistroBooleanPreference(
        prefs, installer_util::master_preferences::kAltShortcutText, true);

  return prefs;
}

installer::Version* setup_util::GetVersionFromDir(
    const std::wstring& chrome_path) {
  LOG(INFO) << "Looking for Chrome version folder under " << chrome_path;
  std::wstring root_path(chrome_path);
  file_util::AppendToPath(&root_path, L"*");

  WIN32_FIND_DATA find_data;
  HANDLE file_handle = FindFirstFile(root_path.c_str(), &find_data);
  BOOL ret = TRUE;
  installer::Version *version = NULL;
  // Here we are assuming that the installer we have is really valid so there
  // can not be two version directories. We exit as soon as we find a valid
  // version directory.
  while (ret) {
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      LOG(INFO) << "directory found: " << find_data.cFileName;
      version = installer::Version::GetVersionFromString(find_data.cFileName);
      if (version) break;
    }
    ret = FindNextFile(file_handle, &find_data);
  }
  FindClose(file_handle);

  return version;
}

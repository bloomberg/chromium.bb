// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_

#include "base/files/file_path.h"

class PrefRegistrySimple;
class Profile;

namespace gfx {
class ImageSkia;
}

namespace chrome {

// TODO(koz/benwells): These functions should be put on an AppList class that
// can be accessed as a global.

// Do any once off initialization needed for the app list.
void InitAppList(Profile* profile);

// Show the app list.
void ShowAppList(Profile* profile);

// Register local state preferences for the app list.
void RegisterAppListPrefs(PrefRegistrySimple* registry);

// Change the profile that the app list is showing.
void SetAppListProfile(const base::FilePath& profile_file_path);

// Get the path of the profile to be used with the app list.
base::FilePath GetAppListProfilePath(const base::FilePath& user_data_dir);

// Dismiss the app list.
void DismissAppList();

// Get the profile the app list is currently showing.
Profile* GetCurrentAppListProfile();

// Returns true if the app list is visible.
bool IsAppListVisible();

// Notify the app list that an extension has started downloading.
void NotifyAppListOfBeginExtensionInstall(
    Profile* profile,
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon);

void NotifyAppListOfDownloadProgress(
    Profile* profile,
    const std::string& extension_id,
    int percent_downloaded);

void NotifyAppListOfExtensionInstallFailure(
    Profile* profile,
    const std::string& extension_id);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_

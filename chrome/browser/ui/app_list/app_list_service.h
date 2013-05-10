// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class AppListControllerDelegate;
class PrefRegistrySimple;
class Profile;

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
}

class AppListService {
 public:
  // Get the AppListService for the current platform and desktop type.
  static AppListService* Get();

  // Call Init for all AppListService instances on this platform.
  static void InitAll(Profile* initial_profile);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual base::FilePath GetAppListProfilePath(
      const base::FilePath& user_data_dir) = 0;

  // Show the app list.
  virtual void ShowAppList(Profile* requested_profile) = 0;

  // Dismiss the app list.
  virtual void DismissAppList() = 0;

  virtual void SetAppListProfile(const base::FilePath& profile_file_path) = 0;

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile() = 0;

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const = 0;

  // Enable the app list. What this does specifically will depend on the host
  // operating system and shell.
  virtual void EnableAppList() = 0;

  // Exposed to allow testing of the controller delegate.
  virtual AppListControllerDelegate* CreateControllerDelegate() = 0;

 protected:
  AppListService() {}
  virtual ~AppListService() {}

  // Do any once off initialization needed for the app list.
  virtual void Init(Profile* initial_profile) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListService);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"

class AppListControllerDelegate;
class CommandLine;
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

  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) = 0;

  static void RecordShowTimings(const CommandLine& command_line);

  // Show the app list.
  virtual void ShowAppList(Profile* requested_profile) = 0;

  // Dismiss the app list.
  virtual void DismissAppList() = 0;

  // TODO(koz): Merge this into ShowAppList().
  virtual void SetAppListProfile(const base::FilePath& profile_file_path) = 0;

  // Show the app list for the profile configured in the user data dir for the
  // current browser process.
  virtual void ShowForSavedProfile() = 0;

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile() = 0;

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const = 0;

  // Enable the app list. What this does specifically will depend on the host
  // operating system and shell.
  virtual void EnableAppList() = 0;

  // Get the window the app list is in, or NULL if the app list isn't visible.
  virtual gfx::NativeWindow GetAppListWindow() = 0;

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

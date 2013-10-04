// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
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

  static void RecordShowTimings(const CommandLine& command_line);

  // Indicates that |callback| should be called next time the app list is
  // painted.
  virtual void SetAppListNextPaintCallback(const base::Closure& callback) = 0;

  // Perform Chrome first run logic. This is executed before Chrome's threads
  // have been created.
  virtual void HandleFirstRun() = 0;

  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) = 0;
  virtual void SetProfilePath(const base::FilePath& profile_path) = 0;

  // Show the app list for the profile configured in the user data dir for the
  // current browser process.
  virtual void Show() = 0;

  // Create the app list UI, and maintain its state, but do not show it.
  virtual void CreateForProfile(Profile* requested_profile) = 0;

  // Show the app list for the given profile. If it differs from the profile the
  // app list is currently showing, repopulate the app list and save the new
  // profile to local prefs as the default app list profile.
  virtual void ShowForProfile(Profile* requested_profile) = 0;

  // Dismiss the app list.
  virtual void DismissAppList() = 0;

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile() = 0;

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const = 0;

  // Enable the app list. What this does specifically will depend on the host
  // operating system and shell.
  virtual void EnableAppList(Profile* initial_profile) = 0;

  // Get the window the app list is in, or NULL if the app list isn't visible.
  virtual gfx::NativeWindow GetAppListWindow() = 0;

  // Creates a platform specific AppListControllerDelegate.
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

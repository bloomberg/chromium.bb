// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include "base/macros.h"

class Profile;
class AppListControllerDelegate;

class AppListService {
 public:
  // Get the AppListService.
  static AppListService* Get();

  // Show the app list for the profile configured in the user data dir for the
  // current browser process.
  virtual void Show() = 0;

  // Dismiss the app list.
  virtual void DismissAppList() = 0;

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile() = 0;

  // Returns true if the app list target is visible.
  virtual bool GetTargetVisibility() const = 0;

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const = 0;

  // Returns a pointer to the platform specific AppListControllerDelegate.
  virtual AppListControllerDelegate* GetControllerDelegate() = 0;

  // Flush pending mojo calls to Ash AppListControllerImpl.
  virtual void FlushForTesting() = 0;

 protected:
  AppListService() {}
  virtual ~AppListService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListService);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

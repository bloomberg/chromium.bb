// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_

#include <string>

#include "base/callback_forward.h"
#include "ui/gfx/image/image.h"

namespace chromeos {

enum class AppType {
  // An Android app.
  ARC,
};

enum class AppsNavigationAction {
  // The current navigation should be cancelled.
  CANCEL,

  // The current navigation should resume.
  RESUME,
};

// Represents the data required to display an app in a picker to the user.
struct IntentPickerAppInfo {
  IntentPickerAppInfo(AppType app_type,
                      const gfx::Image& img,
                      const std::string& launch,
                      const std::string& name);

  // TODO(crbug.com/824598): make this type move-only to avoid unnecessary
  // copies.
  IntentPickerAppInfo(const IntentPickerAppInfo& app_info);

  // The type of app that this object represents.
  AppType type;

  // The icon to be displayed for this app in the picker.
  gfx::Image icon;

  // The string used to launch this app. Represents an Android package name
  // when type is ARC.
  std::string launch_name;

  // The string shown to the user to identify this app in the intent picker.
  std::string display_name;
};

// Callback to allow app-platform-specific code to asynchronously signal what
// action should be taken for the current navigation, and provide a list of apps
// which can handle the navigation.
using AppsNavigationCallback =
    base::OnceCallback<void(AppsNavigationAction action,
                            const std::vector<IntentPickerAppInfo>& apps)>;

// Callback to allow app-platform-specific code to asynchronously provide a list
// of apps which can handle the navigation.
using QueryAppsCallback =
    base::OnceCallback<void(const std::vector<IntentPickerAppInfo>& apps)>;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_

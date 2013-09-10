// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_APP_SHIM_LAUNCH_H_
#define APPS_APP_SHIM_APP_SHIM_LAUNCH_H_

namespace apps {

enum AppShimLaunchType {
  // Process the app shim's LaunchAppmessage and associate the shim with the
  // given profile and app id.
  APP_SHIM_LAUNCH_REGISTER_ONLY = 0,
  // Do the above and launch the app.
  APP_SHIM_LAUNCH_NORMAL,
  // Counter and end marker.
  APP_SHIM_LAUNCH_NUM_TYPES
};

enum AppShimLaunchResult {
  // App launched successfully.
  APP_SHIM_LAUNCH_SUCCESS = 0,
  // There is already a host registered for this app.
  APP_SHIM_LAUNCH_DUPLICATE_HOST,
  // The profile was not found.
  APP_SHIM_LAUNCH_PROFILE_NOT_FOUND,
  // The app was not found.
  APP_SHIM_LAUNCH_APP_NOT_FOUND,
  // Counter and end marker.
  APP_SHIM_LAUNCH_NUM_RESULTS
};

enum AppShimFocusType {
  // Just focus the app.
  APP_SHIM_FOCUS_NORMAL = 0,
  // Focus the app or launch it if it has no windows open.
  APP_SHIM_FOCUS_REOPEN,
  // Open the given file in the app.
  APP_SHIM_FOCUS_OPEN_FILES,
  // Counter and end marker.
  APP_SHIM_FOCUS_NUM_TYPES
};

}  // namespace apps

#endif  // APPS_APP_SHIM_APP_SHIM_LAUNCH_H_

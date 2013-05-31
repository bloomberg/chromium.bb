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
};

}  // namespace apps

#endif  // APPS_APP_SHIM_APP_SHIM_LAUNCH_H_

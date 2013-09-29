// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/pref_names.h"

namespace apps {

namespace prefs {

// A boolean that tracks whether apps are allowed to enter fullscreen mode.
extern const char kAppFullscreenAllowed[] =
    "apps.fullscreen.allowed";

// If set, the user requested to launch the app with this extension id while
// in Metro mode, and then relaunched to Desktop mode to start it.
const char kAppLaunchForMetroRestart[] = "apps.app_launch_for_metro_restart";

// Set with |kAppLaunchForMetroRestart|, the profile whose loading triggers
// launch of the specified app when restarting Chrome in desktop mode.
const char kAppLaunchForMetroRestartProfile[] =
    "apps.app_launch_for_metro_restart_profile";

}  // namespace prefs

}  // namespace apps

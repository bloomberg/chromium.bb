// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/pref_names.h"

namespace apps {

namespace prefs {

// Local state caching knowledge of whether the app launcher is installed.
const char kAppLauncherIsEnabled[] =
    "apps.app_launcher.should_show_apps_page";

// If set, the user requested to launch the app with this extension id while
// in Metro mode, and then relaunched to Desktop mode to start it.
const char kAppLaunchForMetroRestart[] = "apps.app_launch_for_metro_restart";

// Set with |kAppLaunchForMetroRestart|, the profile whose loading triggers
// launch of the specified app when restarting Chrome in desktop mode.
const char kAppLaunchForMetroRestartProfile[] =
    "apps.app_launch_for_metro_restart_profile";

}  // namespace prefs

}  // namespace apps

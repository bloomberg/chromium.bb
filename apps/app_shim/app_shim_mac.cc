// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_mac.h"

#include "apps/app_launcher.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace apps {

bool IsAppShimsEnabled() {
  return IsAppLauncherEnabled() ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppShims);
}

}  // namespace apps

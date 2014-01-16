// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_mac.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace apps {

bool IsAppShimsEnabled() {
  // Disable app shims in tests because shims created in ~/Applications will not
  // be cleaned up.
  return !(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableAppShims));
}

}  // namespace apps

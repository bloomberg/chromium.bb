// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/app_mode_utils.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool IsRunningInAppMode() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kKioskMode) ||
      (command_line->HasSwitch(switches::kForceAppMode) &&
      command_line->HasSwitch(switches::kAppId));
}

}  // namespace switches

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/app_mode_utils.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool IsCommandAllowedInAppMode(int command_id) {
  DCHECK(IsRunningInForcedAppMode());

  const int kAllowed[] = {
      IDC_BACK,
      IDC_FORWARD,
      IDC_BACKSPACE_BACK,
      IDC_BACKSPACE_FORWARD,
      IDC_RELOAD,
      IDC_STOP,
      IDC_RELOAD_BYPASSING_CACHE,
      IDC_RELOAD_CLEARING_CACHE,
      IDC_CUT,
      IDC_COPY,
      IDC_PASTE,
      IDC_ZOOM_PLUS,
      IDC_ZOOM_NORMAL,
      IDC_ZOOM_MINUS,
  };

  for (size_t i = 0; i < arraysize(kAllowed); ++i) {
    if (kAllowed[i] == command_id)
      return true;
  }

  return false;
}

bool IsRunningInAppMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kKioskMode) ||
         IsRunningInForcedAppMode();
}

bool IsRunningInForcedAppMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return (command_line->HasSwitch(switches::kForceAppMode) &&
          command_line->HasSwitch(switches::kAppId)) ||
         command_line->HasSwitch(switches::kForceAndroidAppMode);
}

}  // namespace chrome

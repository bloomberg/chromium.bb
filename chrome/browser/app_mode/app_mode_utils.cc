// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/app_mode_utils.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool IsCommandAllowedInAppMode(int command_id) {
  DCHECK(IsRunningInForcedAppMode());

  const int kAllowed[] = {
    IDC_BACK,
    IDC_FORWARD,
    IDC_RELOAD,
    IDC_STOP,
    IDC_RELOAD_IGNORING_CACHE,
    IDC_RELOAD_CLEARING_CACHE,
    IDC_CUT,
    IDC_COPY,
    IDC_COPY_URL,
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
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kKioskMode) ||
      IsRunningInForcedAppMode();
}

bool IsRunningInForcedAppMode() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kForceAppMode) &&
      command_line->HasSwitch(switches::kAppId);
}

bool ShouldForceFullscreenApp() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return IsRunningInForcedAppMode() &&
      !command_line->HasSwitch(switches::kDisableFullscreenApp);
}

}  // namespace switches

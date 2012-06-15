// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

namespace ash {
namespace switches {

// Show only apps result in app list search.
// TODO(xiyuan): Create an app_list_switches.cc to put all app list switches.
const char kAppListShowAppsOnly[] = "app-list-show-apps-only";

// Enables the Oak tree viewer.
const char kAshEnableOak[] = "ash-enable-oak";

// Enable extended desktop.
const char kAshExtendedDesktop[] = "ash-extended-desktop";

// Use Ash notifications.
const char kAshNotify[] = "ash-notify";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// If present animations are disabled.
const char kAshWindowAnimationsDisabled[] = "ash-window-animations-disabled";

// Use Google-style dialog box frames.
const char kAuraGoogleDialogFrames[] = "aura-google-dialog-frames";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Avoid drawing drop shadows under windows.
const char kAuraNoShadows[] = "aura-no-shadows";

// Use Aura to manage windows of type WINDOW_TYPE_PANEL.
const char kAuraPanelManager[] = "aura-panels";

}  // namespace switches
}  // namespace ash

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

namespace ash {
namespace switches {

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enables the Oak tree viewer.
const char kAshEnableOak[] = "ash-enable-oak";

// Enable extended desktop.
const char kAshExtendedDesktop[] = "ash-extended-desktop";

// Disable using Ash notifications.
const char kAshNotifyDisabled[] = "ash-notify-disabled";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// Enables virtual screen coordinate system.
const char kAshVirtualScreenCoordinates[] = "ash-virtual-screen-coordinates";

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

}  // namespace switches
}  // namespace ash

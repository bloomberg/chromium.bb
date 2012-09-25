// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

namespace ash {
namespace switches {

// Variation of boot animation that uses Tween::EASE_OUT_2.
const char kAshBootAnimationFunction2[] = "ash-boot-animation-function2";

// Variation of boot animation that uses Tween::EASE_OUT_3.
const char kAshBootAnimationFunction3[] = "ash-boot-animation-function3";

// Constrains the pointer movement within a root window on desktop.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Disables Workspace2.
const char kAshDisableWorkspace2[] = "ash-disable-workspace2";

// Disables boot animation v2, go back to v1.
const char kAshDisableBootAnimation2[] = "ash-disable-boot-animation2";

// Enable advanced gestures (e.g. for window management).
const char kAshEnableAdvancedGestures[] = "ash-enable-advanced-gestures";

// Enables the Oak tree viewer.
const char kAshEnableOak[] = "ash-enable-oak";

// Enables showing the tray bubble by dragging on the shelf.
const char kAshEnableTrayDragging[] = "ash-enable-tray-dragging";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kAshSecondaryDisplayLayout[] = "ash-secondary-display-layout";

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

}  // namespace switches
}  // namespace ash

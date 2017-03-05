// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/ash_switches.h"

#include "base/command_line.h"
#include "base/sys_info.h"

namespace ash {
namespace switches {

// Enables adjustable large cursor.
const char kAshAdjustableLargeCursor[] = "ash-adjustable-large-cursor";

// Enables an animated transition from the boot splash screen (Chrome logo on a
// white background) to the login screen.  Implies
// |kAshCopyHostBackgroundAtBoot| and doesn't make much sense if used in
// conjunction with |kDisableBootAnimation| (since the transition begins at the
// same time as the white/grayscale login screen animation).
const char kAshAnimateFromBootSplashScreen[] =
    "ash-animate-from-boot-splash-screen";

// Copies the host window's content to the system background layer at startup.
// Can make boot slightly slower, but also hides an even-longer awkward period
// where we display a white background if the login wallpaper takes a long time
// to load.
const char kAshCopyHostBackgroundAtBoot[] = "ash-copy-host-background-at-boot";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enable keyboard shortcuts used by developers only.
const char kAshDeveloperShortcuts[] = "ash-dev-shortcuts";

// Disables the window backdrops normally used in maximize mode (TouchView).
const char kAshDisableMaximizeModeWindowBackdrop[] =
    "ash-disable-maximize-mode-window-backdrop";

// Disable the support for WebContents to lock the screen orientation.
const char kAshDisableScreenOrientationLock[] =
    "ash-disable-screen-orientation-lock";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the palette on every display, instead of only the internal one.
const char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// Enables docking windows to the right or left (not to be confused with snapped
// windows).
const char kAshEnableDockedWindows[] = "ash-enable-docked-windows";

// Enables the observation of accelerometer events to enter touch-view mode.
const char kAshEnableTouchView[] = "enable-touchview";

// Enables mirrored screen.
const char kAshEnableMirroredScreen[] = "ash-enable-mirrored-screen";

// Enables the palette next to the status area.
const char kAshForceEnablePalette[] = "ash-force-enable-palette";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
const char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Enables the shelf color to be derived from the wallpaper.
const char kAshShelfColor[] = "ash-shelf-color";
const char kAshShelfColorLightMuted[] = "light_muted";
const char kAshShelfColorLightVibrant[] = "light_vibrant";
const char kAshShelfColorNormalMuted[] = "normal_muted";
const char kAshShelfColorNormalVibrant[] = "normal_vibrant";
const char kAshShelfColorDarkMuted[] = "dark_muted";
const char kAshShelfColorDarkVibrant[] = "dark_vibrant";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Constrains the pointer movement within a root window on desktop.
bool ConstrainPointerToRoot() {
  const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

  return base::SysInfo::IsRunningOnChromeOS() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kAshConstrainPointerToRoot);
}

bool DockedWindowsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableDockedWindows);
}

}  // namespace switches
}  // namespace ash

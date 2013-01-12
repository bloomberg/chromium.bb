// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

namespace ash {
namespace switches {

// Enables an animated transition from the boot splash screen (Chrome logo on a
// white background) to the login screen.  Implies
// |kAshCopyHostBackgroundAtBoot| and doesn't make much sense if used in
// conjunction with |kDisableBootAnimation| (since the transition begins at the
// same time as the white/grayscale login screen animation).
const char kAshAnimateFromBootSplashScreen[] =
    "ash-animate-from-boot-splash-screen";

// Variation of boot animation that uses Tween::EASE_OUT_2.
const char kAshBootAnimationFunction2[] = "ash-boot-animation-function2";

// Variation of boot animation that uses Tween::EASE_OUT_3.
const char kAshBootAnimationFunction3[] = "ash-boot-animation-function3";

// Constrains the pointer movement within a root window on desktop.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Copies the host window's content to the system background layer at startup.
// Can make boot slightly slower, but also hides an even-longer awkward period
// where we display a white background if the login wallpaper takes a long time
// to load.
const char kAshCopyHostBackgroundAtBoot[] = "ash-copy-host-background-at-boot";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Disable support for auto window placement.
const char kAshDisableAutoWindowPlacement[] =
    "ash-enable-auto-window-placement";

// Disables the limitter to throttle how quickly a user
// can change display settings.
const char kAshDisableDisplayChangeLimiter[] =
    "ash-disable-display-change-limiter";

// Disables boot animation v2, go back to v1.
const char kAshDisableBootAnimation2[] = "ash-disable-boot-animation2";

// Disables panel fitting (used for mirror mode).
const char kAshDisablePanelFitting[] = "ash-disable-panel-fitting";

// Enable advanced gestures (e.g. for window management).
const char kAshEnableAdvancedGestures[] = "ash-enable-advanced-gestures";

// Enable workspace switching via a three finger vertical scroll.
const char kAshEnableWorkspaceScrubbing[] = "ash-enable-workspace-scrubbing";

#if defined(OS_LINUX)
// Enable memory monitoring.
const char kAshEnableMemoryMonitor[] = "ash-enable-memory-monitor";
#endif

// Enable the per application grouping version of the launcher.
const char kAshEnablePerAppLauncher[] = "ash-enable-per-app-launcher";

// Enables the Oak tree viewer.
const char kAshEnableOak[] = "ash-enable-oak";

// Enables showing the tray bubble by dragging on the shelf.
const char kAshEnableTrayDragging[] = "ash-enable-tray-dragging";

// Hides the small tab indicators at the top of the screen.
const char kAshImmersiveHideTabIndicators[] =
    "ash-immersive-hide-tab-indicators";

// Enables creating a launcher per display.
const char kAshLauncherPerDisplay[] = "ash-launcher-per-display";

// If present new lock animations are enabled.
const char kAshDisableNewLockAnimations[] = "ash-disable-new-lock-animations";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kAshSecondaryDisplayLayout[] = "ash-secondary-display-layout";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Avoid drawing drop shadows under windows.
const char kAuraNoShadows[] = "aura-no-shadows";

}  // namespace switches
}  // namespace ash

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

// Disable auto window maximization logic.
const char kAshDisableAutoMaximizing[] = "ash-disable-auto-maximizing";

// Disable support for auto window placement.
const char kAshDisableAutoWindowPlacement[] =
    "ash-enable-auto-window-placement";

// Disables boot animation v2, go back to v1.
const char kAshDisableBootAnimation2[] = "ash-disable-boot-animation2";

// Disables the limitter to throttle how quickly a user
// can change display settings.
const char kAshDisableDisplayChangeLimiter[] =
    "ash-disable-display-change-limiter";

// Disables creating a launcher per display.
const char kAshDisableLauncherPerDisplay[] = "ash-disable-launcher-per-display";

// Disable the new cras audio handler.
const char kAshDisableNewAudioHandler[] = "ash-disable-new-audio-handler";

// If present new lock animations are enabled.
const char kAshDisableNewLockAnimations[] = "ash-disable-new-lock-animations";

// Disable new network handlers in the status area.
const char kAshDisableNewNetworkStatusArea[] =
    "ash-disable-new-network-status-area";

// Disable the per application grouping version of the launcher.
const char kAshDisablePerAppLauncher[] = "ash-disable-per-app-launcher";

// Disables display rotation.
const char kAshDisableDisplayRotation[] = "ash-disable-display-rotation";

// Disable immersive fullscreen mode, regardless of default setting.
const char kAshDisableImmersiveFullscreen[] =
    "ash-disable-immersive-fullscreen";

// Disables ui scaling.
const char kAshDisableUIScaling[] = "ash-disable-ui-scaling";

// Extend the status tray volume item to allow the user to choose an audio
// input and output device.
const char kAshEnableAudioDeviceMenu[] =
    "ash-enable-audio-device-menu";

// Enable advanced gestures (e.g. for window management).
const char kAshEnableAdvancedGestures[] = "ash-enable-advanced-gestures";

// Always enable brightness control. Used by machines that don't report their
// main monitor as internal.
const char kAshEnableBrightnessControl[] = "ash-enable-brightness-control";

// Enable immersive fullscreen mode, regardless of default setting.
const char kAshEnableImmersiveFullscreen[] = "ash-enable-immersive-fullscreen";

#if defined(OS_LINUX)
// Enable memory monitoring.
const char kAshEnableMemoryMonitor[] = "ash-enable-memory-monitor";
#endif

// Enables the Oak tree viewer.
const char kAshEnableOak[] = "ash-enable-oak";

// Enables "sticky" edges instead of "snap-to-edge"
const char kAshEnableStickyEdges[] = "ash-enable-sticky-edges";

// Enables showing the tray bubble by dragging on the shelf.
const char kAshEnableTrayDragging[] = "ash-enable-tray-dragging";

// Enable workspace switching via a three finger vertical scroll.
const char kAshEnableWorkspaceScrubbing[] = "ash-enable-workspace-scrubbing";

// Sets a window size, optional position, and optional scale factor.
// "1024x768" creates a window of size 1024x768.
// "100+200-1024x768" positions the window at 100,200.
// "1024x768*2" sets the scale factor to 2 for a high DPI display.
const char kAshHostWindowBounds[] = "ash-host-window-bounds";

// Hides the small tab indicators at the top of the screen during immersive
// fullscreen mode.
const char kAshImmersiveHideTabIndicators[] =
    "ash-immersive-hide-tab-indicators";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kAshSecondaryDisplayLayout[] = "ash-secondary-display-layout";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// Uses the 1st display in --ash-host-window-bounds as internal display.
// This is for debugging on linux desktop.
const char kAshUseFirstDisplayAsInternal[] =
    "ash-use-first-display-as-internal";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

#if defined(OS_WIN)
// Force Ash to open its root window on the desktop, even on Windows 8 where
// it would normally end up in metro.
const char kForceAshToDesktop[] = "ash-force-desktop";
#endif

// Enables a mode which enforces all browser & application windows to be created
// in maximized mode.
const char kForcedMaximizeMode[] = "forced-maximize-mode";

}  // namespace switches
}  // namespace ash

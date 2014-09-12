// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

#include "base/command_line.h"

namespace ash {
namespace switches {

// Enables an animated transition from the boot splash screen (Chrome logo on a
// white background) to the login screen.  Implies
// |kAshCopyHostBackgroundAtBoot| and doesn't make much sense if used in
// conjunction with |kDisableBootAnimation| (since the transition begins at the
// same time as the white/grayscale login screen animation).
const char kAshAnimateFromBootSplashScreen[] =
    "ash-animate-from-boot-splash-screen";

// Constrains the pointer movement within a root window on desktop.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Copies the host window's content to the system background layer at startup.
// Can make boot slightly slower, but also hides an even-longer awkward period
// where we display a white background if the login wallpaper takes a long time
// to load.
const char kAshCopyHostBackgroundAtBoot[] = "ash-copy-host-background-at-boot";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kAshDefaultWallpaperIsOem[] = "ash-default-wallpaper-is-oem";

// Default wallpaper to use (as paths to trusted, non-user-writable JPEG files).
const char kAshDefaultWallpaperLarge[] = "ash-default-wallpaper-large";
const char kAshDefaultWallpaperSmall[] = "ash-default-wallpaper-small";

// Disables LockLayoutManager used for LockScreenContainer, return back to
// WorkspaceLayoutManager.
const char kAshDisableLockLayoutManager[] = "ash-disable-lock-layout-manager";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

#if defined(OS_CHROMEOS)
// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";
#endif

// Enables text filtering with the keyboard in Overview Mode.
const char kAshDisableTextFilteringInOverviewMode[] =
    "ash-disable-text-filtering-in-overview-mode";

// Enables quick, non-cancellable locking of the screen when in maximize mode.
const char kAshEnablePowerButtonQuickLock[] =
    "ash-enable-power-button-quick-lock";

// Enables software based mirroring.
const char kAshEnableSoftwareMirroring[] = "ash-enable-software-mirroring";

// Enables touch view testing.
// TODO(skuhne): Remove TOGGLE_TOUCH_VIEW_TESTING accelerator once this
// flag is removed.
const char kAshEnableTouchViewTesting[] = "ash-enable-touch-view-testing";

// Enables additional visual feedback to touch input.
const char kAshEnableTouchViewTouchFeedback[] =
    "ash-enable-touch-view-touch-feedback";

// When this flag is set, system sounds will be played whether the
// ChromeVox is enabled or not.
const char kAshEnableSystemSounds[] = "ash-enable-system-sounds";

// Enables showing the tray bubble by dragging on the shelf.
const char kAshEnableTrayDragging[] = "ash-enable-tray-dragging";

// Wallpaper to use in guest mode (as paths to trusted, non-user-writable JPEG
// files).
const char kAshGuestWallpaperLarge[] = "ash-guest-wallpaper-large";
const char kAshGuestWallpaperSmall[] = "ash-guest-wallpaper-small";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
const char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Sets a window size, optional position, and optional scale factor.
// "1024x768" creates a window of size 1024x768.
// "100+200-1024x768" positions the window at 100,200.
// "1024x768*2" sets the scale factor to 2 for a high DPI display.
const char kAshHostWindowBounds[] = "ash-host-window-bounds";

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

}  // namespace switches
}  // namespace ash

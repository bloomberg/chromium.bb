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

// UI to show preferred networks in the status area (for testing).
const char kAshDebugShowPreferredNetworks[] =
    "ash-debug-show-preferred-networks";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kAshDefaultWallpaperIsOem[] = "ash-default-wallpaper-is-oem";

// Default wallpaper to use (as paths to trusted, non-user-writable JPEG files).
const char kAshDefaultWallpaperLarge[] = "ash-default-wallpaper-large";
const char kAshDefaultWallpaperSmall[] = "ash-default-wallpaper-small";

// Use the normal visual style for the caption buttons (minimize, maximize,
// restore, close).
const char kAshDisableAlternateFrameCaptionButtonStyle[] =
    "ash-disable-alternate-caption-button";

// Disable the alternate shelf layout.
const char kAshDisableAlternateShelfLayout[] =
    "ash-disable-alternate-shelf-layout";

#if defined(OS_CHROMEOS)
// Disable the status tray volume menu for allowing the user to choose an audio
// input and output device.
const char kAshDisableAudioDeviceMenu[] =
    "ash-disable-audio-device-menu";
#endif

// Disable ability to dock windows at the desktop edge.
const char kAshDisableDockedWindows[] = "ash-disable-docked-windows";

// Disallow items to be dragged from the app launcher list into the launcher.
const char kAshDisableDragAndDropAppListToLauncher[] =
    "ash-disable-drag-and-drop-applist-to-launcher";

// Disable dragging items off the shelf to unpin them.
const char kAshDisableDragOffShelf[] = "ash-disable-drag-off-shelf";

// Disables overview mode for window switching.
const char kAshDisableOverviewMode[] = "ash-disable-overview-mode";

#if defined(OS_CHROMEOS)
// Disable the notification when a low-power USB charger is connected.
const char kAshDisableUsbChargerNotification[] =
    "ash-disable-usb-charger-notification";

// TODO(jamescook): Remove this unused flag. It exists only to allow the
// "Enable audio device menu" about:flags item to have the tri-state
// default/enabled/disabled UI.
const char kAshEnableAudioDeviceMenu[] = "ash-enable-audio-device-menu";
#endif  // defined(OS_CHROMEOS)

// Enable advanced gestures (e.g. for window management).
const char kAshEnableAdvancedGestures[] = "ash-enable-advanced-gestures";

// Use alternate visual style for the caption buttons (minimize, maximize,
// restore, close). The alternate style:
// - Adds a dedicated button for minimize.
// - Removes the maximize button's help bubble.
const char kAshEnableAlternateFrameCaptionButtonStyle[] =
    "ash-enable-alternate-caption-button";

// Always enable brightness control. Used by machines that don't report their
// main monitor as internal.
const char kAshEnableBrightnessControl[] = "ash-enable-brightness-control";

// Enables putting all windows into immersive fullscreen via <F4>.
const char kAshEnableImmersiveFullscreenForAllWindows[] =
    "ash-enable-immersive-all-windows";

// Enables putting only browser windows into immersive fullscreen via <F4>.
// <F4> puts all other windows into non-immersive fullscreen.
const char kAshEnableImmersiveFullscreenForBrowserOnly[] =
    "ash-enable-immersive-browser-only";

#if defined(OS_LINUX)
// Enable memory monitoring.
const char kAshEnableMemoryMonitor[] = "ash-enable-memory-monitor";
#endif

#if defined(OS_CHROMEOS)
// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the multi user icons in the system tray.
const char kAshEnableMultiUserTray[] = "ash-enable-multi-user-tray";
#endif

// Enables software based mirroring.
const char kAshEnableSoftwareMirroring[] = "ash-enable-software-mirroring";

// When this flag is set, system sounds will be played whether the
// ChromeVox is enabled or not.
const char kAshEnableSystemSounds[] = "ash-enable-system-sounds";

// Enables showing the tray bubble by dragging on the shelf.
const char kAshEnableTrayDragging[] = "ash-enable-tray-dragging";

// Forces chrome to use mirror mode when an external display is connected.
const char kAshForceMirrorMode[] = "ash-force-mirror-mode";

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

// Specifies the delay in milliseconds before beginning overview mode after
// getting an alt tab keypress.
const char kAshOverviewDelayOnAltTab[] = "ash-overview-delay-on-alt-tab";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kAshSecondaryDisplayLayout[] = "ash-secondary-display-layout";

// Use the old behavior where the user can pick the width of a side maximized
// window. The user selects the width of the side maximized window based on how
// far off the edge of the work area they drag the window.
const char kAshMultipleSnapWindowWidths[] = "ash-multiple-snap-window-widths";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// Use alternate layout of the shelf for testing a new look and feel:
// Slightly smaller profile, only 2 states for the "bar highlight" on
// launcher buttons, app list icon with more visible state indication,
// app list icon repositionable and defaulting as 1st item in shelf,
// more visible state indication for background on status area.
// crbug's [244983, 244990, 244994, 245005, 245012]
const char kAshUseAlternateShelfLayout[] = "ash-use-alternate-shelf";

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

bool UseAlternateFrameCaptionButtonStyle() {
  // For the sake of simplicity, the alternate caption button style is only
  // used if snapped windows are always 50% of the screen's width.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kAshEnableAlternateFrameCaptionButtonStyle) &&
      !command_line->HasSwitch(kAshMultipleSnapWindowWidths);
}

bool UseAlternateShelfLayout() {
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(kAshDisableAlternateShelfLayout);
}

bool UseDragOffShelf() {
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(kAshDisableDragOffShelf);
}

bool UseImmersiveFullscreenForAllWindows() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      kAshEnableImmersiveFullscreenForBrowserOnly);
}

bool UseMultiUserTray() {
#if defined(OS_CHROMEOS)
  // TODO(skuhne): If this gets removed for good, remove also
  // |SystemTray::user_items_| and the use of it.
  return CommandLine::ForCurrentProcess()->HasSwitch(kAshEnableMultiUserTray);
#else
  return false;
#endif
}

bool UseOverviewMode() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(kAshDisableOverviewMode);
}

bool UseDockedWindows() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(kAshDisableDockedWindows);
}

bool ShowAudioDeviceMenu() {
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(kAshDisableAudioDeviceMenu);
#else
  return false;
#endif
}

#if defined(OS_CHROMEOS)
bool UseUsbChargerNotification() {
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(kAshDisableUsbChargerNotification);
}
#endif

}  // namespace switches
}  // namespace ash

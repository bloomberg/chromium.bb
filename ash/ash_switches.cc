// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"

#include "base/command_line.h"
#include "base/sys_info.h"

namespace ash {
namespace switches {

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

// Disable the Night Light feature.
const char kAshDisableNightLight[] = "ash-disable-night-light";

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

// Enables the observation of accelerometer events to enter touch-view mode.
const char kAshEnableTouchView[] = "enable-touchview";

// Enables mirrored screen.
const char kAshEnableMirroredScreen[] = "ash-enable-mirrored-screen";

// Enables display scale tray settings. This uses force-device-scale-factor flag
// to modify the dsf of the device to any non discrete value.
const char kAshEnableScaleSettingsTray[] = "ash-enable-scale-settings-tray";

// Disables a smoother animation for screen rotation.
const char kAshDisableSmoothScreenRotation[] =
    "ash-disable-smooth-screen-rotation";

// Specifies the estimated time (in milliseconds) from VSYNC event until when
// visible light can be noticed by the user.
const char kAshEstimatedPresentationDelay[] =
    "ash-estimated-presentation-delay";

// Enables the stylus tools next to the status area.
const char kAshForceEnableStylusTools[] = "force-enable-stylus-tools";

// Enables required things for the selected UI mode, regardless of whether the
// Chromebook is currently in the selected UI mode.
const char kAshForceTabletMode[] = "force-tablet-mode";

// Values for the kAshForceTabletMode flag.
const char kAshForceTabletModeAuto[] = "auto";
const char kAshForceTabletModeClamshell[] = "clamshell";
const char kAshForceTabletModeTouchView[] = "touch_view";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
const char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Enables the shelf color to be derived from the wallpaper.
const char kAshShelfColor[] = "ash-shelf-color";
const char kAshShelfColorEnabled[] = "enabled";
const char kAshShelfColorDisabled[] = "disabled";

// The color scheme to be used when the |kAshShelfColor| feature is enabled.
const char kAshShelfColorScheme[] = "ash-shelf-color-scheme";
const char kAshShelfColorSchemeLightMuted[] = "light_muted";
const char kAshShelfColorSchemeLightVibrant[] = "light_vibrant";
const char kAshShelfColorSchemeNormalMuted[] = "normal_muted";
const char kAshShelfColorSchemeNormalVibrant[] = "normal_vibrant";
const char kAshShelfColorSchemeDarkMuted[] = "dark_muted";
const char kAshShelfColorSchemeDarkVibrant[] = "dark_vibrant";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// By default we use classic IME (i.e. InputMethodChromeOS) in kMus. This flag
// enables the IME service (i.e. InputMethodMus) instead.
const char kUseIMEService[] = "use-ime-service";

// Constrains the pointer movement within a root window on desktop.
bool ConstrainPointerToRoot() {
  const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

  return base::SysInfo::IsRunningOnChromeOS() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kAshConstrainPointerToRoot);
}

}  // namespace switches
}  // namespace ash

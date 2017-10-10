// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"

#include "base/command_line.h"

namespace ash {
namespace switches {

// Force the pointer (cursor) position to be kept inside root windows.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enable keyboard shortcuts used by developers only.
const char kAshDeveloperShortcuts[] = "ash-dev-shortcuts";

// Use a single in-process shelf data model shared between Chrome and Ash.
// This only applies to the Classic Ash and Mus configs; synchronization between
// two models is required when running the Mash config via --mash.
const char kAshDisableShelfModelSynchronization[] =
    "ash-disable-shelf-model-synchronization";

// Disables autohide titlebars feature. With this flag disabled, apps in tablet
// mode will have visible titlebars instead of autohidden titlebars.
// TODO(crbug.com/764393): Remove this flag in M66/M67.
const char kAshDisableTabletAutohideTitlebars[] =
    "ash-disable-tablet-autohide-titlebars";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enable the Night Light feature.
const char kAshEnableNightLight[] = "ash-enable-night-light";

// Enables the palette on every display, instead of only the internal one.
const char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// Enables the split view on tablet mode.
const char kAshEnableTabletSplitView[] = "enable-tablet-splitview";

// Enables the observation of accelerometer events to enter tablet
// mode.  The flag is "enable-touchview" not "enable-tabletmode" as this
// is used to enable tablet mode on convertible devices.
const char kAshEnableTabletMode[] = "enable-touchview";

// Enable the wayland server.
const char kAshEnableWaylandServer[] = "ash-enable-wayland-server";

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
const char kAshUiMode[] = "force-tablet-mode";

// Values for the kAshUiMode flag.
const char kAshUiModeAuto[] = "auto";
const char kAshUiModeClamshell[] = "clamshell";
const char kAshUiModeTablet[] = "touch_view";

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

// Forces non-tablet-style power button behavior even if the device has a
// convertible form factor.
const char kForceClamshellPowerButton[] = "force-clamshell-power-button";

// Whether this device has an internal stylus.
const char kHasInternalStylus[] = "has-internal-stylus";

// Number of recent accelerometer samples to examine to determine if a power
// button event was spurious.
const char kSpuriousPowerButtonWindow[] = "spurious-power-button-window";

// Number of recent acceleration samples that must meet or exceed the threshold
// in order for a power button event to be considered spurious.
const char kSpuriousPowerButtonAccelCount[] =
    "spurious-power-button-accel-count";

// Threshold (in m/s^2, disregarding gravity) that screen acceleration must meet
// or exceed for a power button event to be considered spurious.
const char kSpuriousPowerButtonScreenAccel[] =
    "spurious-power-button-screen-accel";

// Threshold (in m/s^2, disregarding gravity) that keyboard acceleration must
// meet or exceed for a power button event to be considered spurious.
const char kSpuriousPowerButtonKeyboardAccel[] =
    "spurious-power-button-keyboard-accel";

// Change in lid angle (i.e. hinge between keyboard and screen) that must be
// exceeded for a power button event to be considered spurious.
const char kSpuriousPowerButtonLidAngleChange[] =
    "spurious-power-button-lid-angle-change";

// By default we use classic IME (i.e. InputMethodChromeOS) in kMus. This flag
// enables the IME service (i.e. InputMethodMus) instead.
const char kUseIMEService[] = "use-ime-service";

bool IsNightLightEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshEnableNightLight);
}

}  // namespace switches
}  // namespace ash

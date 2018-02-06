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

// Disables the dimming and blur of the wallpaper on login and lock screens.
const char kAshDisableLoginDimAndBlur[] = "ash-disable-login-dim-and-blur";

// Use a single in-process shelf data model shared between Chrome and Ash.
// This only applies to the Classic Ash and Mus configs; synchronization between
// two models is required when running the Mash config via --mash.
const char kAshDisableShelfModelSynchronization[] =
    "ash-disable-shelf-model-synchronization";

// Disables a smoother animation for screen rotation.
const char kAshDisableSmoothScreenRotation[] =
    "ash-disable-smooth-screen-rotation";

// Disables autohide titlebars feature. With this flag disabled, apps in tablet
// mode will have visible titlebars instead of autohidden titlebars.
// TODO(crbug.com/764393): Remove this flag in M66/M67.
const char kAshDisableTabletAutohideTitlebars[] =
    "ash-disable-tablet-autohide-titlebars";

// Disables the split view on tablet mode.
const char kAshDisableTabletSplitView[] = "disable-tablet-splitview";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables Backbutton on frame for v1 apps.
// TODO(oshima): Remove this once the feature is launched. crbug.com/749713.
const char kAshEnableV1AppBackButton[] = "ash-enable-v1-app-back-button";

// Enables move window between displays accelerators.
// TODO(warx): Remove this once the feature is launched. crbug.com/773749.
const char kAshEnableDisplayMoveWindowAccels[] =
    "ash-enable-display-move-window-accels";

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this once the feature is launched.
// https://crbug.com/709824.
const char kAshEnableDockedMagnifier[] = "ash-enable-docked-magnifier";

// Enables keyboard shortcut viewer.
// TODO(wutao): Remove this once the feature is launched. crbug.com/768932.
const char kAshEnableKeyboardShortcutViewer[] =
    "ash-enable-keyboard-shortcut-viewer";

// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the new overview ui.
// TODO(sammiequon): Remove this once the feature is launched. crbug.com/782330.
const char kAshEnableNewOverviewUi[] = "ash-enable-new-overview-ui";

// Enable the Night Light feature.
const char kAshEnableNightLight[] = "ash-enable-night-light";

// Enables the palette on every display, instead of only the internal one.
const char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// Enables persistent window bounds in multi-displays scenario.
// TODO(warx): Remove this once the feature is launched. crbug.com/805046.
const char kAshEnablePersistentWindowBounds[] =
    "ash-enable-persistent-window-bounds";

// Enables the sidebar.
const char kAshSidebarEnabled[] = "enable-ash-sidebar";
const char kAshSidebarDisabled[] = "disable-ash-sidebar";

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

// Enables new implementation of touch support for screen magnification.
const char kAshNewTouchSupportForScreenMagnification[] =
    "ash-new-touch-support-for-screen-magnification";

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

// If true, a long press of the power button in tablet mode will show the power
// button menu.
const char kShowPowerButtonMenu[] = "show-power-button-menu";

// Draws a circle at each touch point, similar to the Android OS developer
// option "Show taps".
const char kShowTaps[] = "show-taps";

// If true, the views login screen will be shown. This will become the default
// in the future.
const char kShowViewsLogin[] = "show-views-login";

// If true, the webui lock screen wil be shown. This is deprecated and will be
// removed in the future.
const char kShowWebUiLock[] = "show-webui-lock";

// Chromebases' touchscreens can be used to wake from suspend, unlike the
// touchscreens on other Chrome OS devices. If set, the touchscreen is kept
// enabled while the screen is off so that it can be used to turn the screen
// back on after it has been turned off for inactivity but before the system has
// suspended.
const char kTouchscreenUsableWhileScreenOff[] =
    "touchscreen-usable-while-screen-off";

// Hides all Message Center notification popups (toasts). Used for testing.
const char kSuppressMessageCenterPopups[] = "suppress-message-center-popups";

// By default we use classic IME (i.e. InputMethodChromeOS) in kMus. This flag
// enables the IME service (i.e. InputMethodMus) instead.
const char kUseIMEService[] = "use-ime-service";

bool IsDisplayMoveWindowAccelsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshEnableDisplayMoveWindowAccels);
}

bool IsDockedMagnifierEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshEnableDockedMagnifier);
}

bool IsNightLightEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshEnableNightLight);
}

bool IsSidebarEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshSidebarEnabled);
}

bool IsUsingViewsLogin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kShowViewsLogin);
}

bool IsUsingViewsLock() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kShowWebUiLock);
}

}  // namespace switches
}  // namespace ash

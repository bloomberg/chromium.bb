// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// There are five classes of accelerators in Ash:
//
// Ash (OS) reserved:
// * Neither packaged apps nor web pages can cancel.
// * For example, power button.
// * See kReservedActions below.
//
// Ash (OS) preferred:
// * Fullscreen window can consume, but normal window can't.
// * For example, Alt-Tab window cycling.
// * See kPreferredActions below.
//
// Chrome OS system keys:
// * For legacy reasons, v1 apps can process and cancel. Otherwise handled
//   directly by Ash.
// * Brightness, volume control, etc.
// * See IsSystemKey() in ash/accelerators/accelerator_filter.cc.
//
// Browser reserved:
// * Packaged apps can cancel but web pages cannot.
// * For example, browser back and forward from first-row function keys.
// * See IsReservedCommandOrKey() in
//   chrome/browser/ui/browser_command_controller.cc.
//
// Browser non-reserved:
// * Both packaged apps and web pages can cancel.
// * For example, selecting tabs by number with Ctrl-1 to Ctrl-9.
// * See kAcceleratorMap in chrome/browser/ui/views/accelerator_table.cc.
//
// In particular, there is not an accelerator processing pass for Ash after
// the browser gets the accelerator.  See crbug.com/285308 for details.
//
// There are also various restrictions on accelerators allowed at the login
// screen, when running in "forced app mode" (like a kiosk), etc. See the
// various kActionsAllowed* below.
//
// Please put if/def sections at the end of the bare section and keep the list
// within each section in alphabetical order.
enum AcceleratorAction {
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
  DEBUG_PRINT_LAYER_HIERARCHY,
  DEBUG_PRINT_VIEW_HIERARCHY,
  DEBUG_PRINT_WINDOW_HIERARCHY,
  DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN,
  DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE,
  DEBUG_TOGGLE_DEVICE_SCALE_FACTOR,
  DEBUG_TOGGLE_SHOW_DEBUG_BORDERS,
  DEBUG_TOGGLE_SHOW_FPS_COUNTER,
  DEBUG_TOGGLE_SHOW_PAINT_RECTS,
  EXIT,
  FOCUS_NEXT_PANE,
  FOCUS_PREVIOUS_PANE,
  FOCUS_SHELF,
  LAUNCH_APP_0,
  LAUNCH_APP_1,
  LAUNCH_APP_2,
  LAUNCH_APP_3,
  LAUNCH_APP_4,
  LAUNCH_APP_5,
  LAUNCH_APP_6,
  LAUNCH_APP_7,
  LAUNCH_LAST_APP,
  MAGNIFY_SCREEN_ZOOM_IN,
  MAGNIFY_SCREEN_ZOOM_OUT,
  MEDIA_NEXT_TRACK,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  NEW_INCOGNITO_WINDOW,
  NEW_TAB,
  NEW_WINDOW,
  NEXT_IME,
  OPEN_FEEDBACK_PAGE,
  PREVIOUS_IME,
  PRINT_UI_HIERARCHIES,
  RESTORE_TAB,
  ROTATE_SCREEN,
  ROTATE_WINDOW,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SCALE_UI_UP,
  SHOW_KEYBOARD_OVERLAY,
  SHOW_MESSAGE_CENTER_BUBBLE,
  SHOW_SYSTEM_TRAY_BUBBLE,
  SHOW_TASK_MANAGER,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_WINDOW_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_FULLSCREEN,
  TOGGLE_MAXIMIZED,
  TOGGLE_OVERVIEW,
  WINDOW_MINIMIZE,
  WINDOW_POSITION_CENTER,
  WINDOW_CYCLE_SNAP_DOCK_LEFT,
  WINDOW_CYCLE_SNAP_DOCK_RIGHT,
#if defined(OS_CHROMEOS)
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  DEBUG_ADD_REMOVE_DISPLAY,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DEBUG_TOGGLE_UNIFIED_DESKTOP,
  DISABLE_CAPS_LOCK,
  DISABLE_GPU_WATCHDOG,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  LOCK_PRESSED,
  LOCK_RELEASED,
  LOCK_SCREEN,
  OPEN_CROSH,
  OPEN_FILE_MANAGER,
  OPEN_GET_HELP,
  POWER_PRESSED,
  POWER_RELEASED,
  SUSPEND,
  SWAP_PRIMARY_DISPLAY,
  SWITCH_TO_NEXT_USER,
  SWITCH_TO_PREVIOUS_USER,
  TOGGLE_CAPS_LOCK,
  TOGGLE_MIRROR_MODE,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_TOUCH_VIEW_TESTING,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  TOUCH_HUD_MODE_CHANGE,
  TOUCH_HUD_PROJECTION_TOGGLE,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#else
  DUMMY_FOR_RESERVED,
#endif
};

struct AcceleratorData {
  bool trigger_on_press;
  ui::KeyboardCode keycode;
  int modifiers;
  AcceleratorAction action;
};

// Gathers the needed data to handle deprecated accelerators.
struct DeprecatedAcceleratorData {
  // The action that has deprecated accelerators.
  AcceleratorAction action;

  // The name of the UMA histogram that will be used to measure the deprecated
  // v.s. new accelerator usage.
  const char* uma_histogram_name;

  // The ID of the localized notification message to show to users informing
  // them about the deprecation.
  int notification_message_id;

  // The ID of the localized old deprecated shortcut key.
  int old_shortcut_id;

  // The ID of the localized new shortcut key.
  int new_shortcut_id;

  // Specifies whether the deprecated accelerator is still enabled to do its
  // associated action.
  bool deprecated_enabled;
};

// This will be used for the UMA stats to measure the how many users are using
// the old v.s. new accelerators.
enum DeprecatedAcceleratorUsage {
  DEPRECATED_USED = 0,     // The deprecated accelerator is used.
  NEW_USED,                // The new accelerator is used.
  DEPRECATED_USAGE_COUNT,  // Maximum value of this enum for histogram use.
};

// Accelerators handled by AcceleratorController.
ASH_EXPORT extern const AcceleratorData kAcceleratorData[];
ASH_EXPORT extern const size_t kAcceleratorDataLength;

#if defined(OS_CHROMEOS)
// The list of the deprecated accelerators.
ASH_EXPORT extern const AcceleratorData kDeprecatedAccelerators[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsLength;

// The list of the actions with deprecated accelerators and the needed data to
// handle them.
ASH_EXPORT extern const DeprecatedAcceleratorData kDeprecatedAcceleratorsData[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsDataLength;
#endif  // defined(OS_CHROMEOS)

// Debug accelerators. Debug accelerators are only enabled when the "Debugging
// keyboard shortcuts" flag (--ash-debug-shortcuts) is enabled. Debug actions
// are always run (similar to reserved actions).
ASH_EXPORT extern const AcceleratorData kDebugAcceleratorData[];
ASH_EXPORT extern const size_t kDebugAcceleratorDataLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT extern const AcceleratorAction kPreferredActions[];
ASH_EXPORT extern const size_t kPreferredActionsLength;

// Actions that are always handled in Ash.
ASH_EXPORT extern const AcceleratorAction kReservedActions[];
ASH_EXPORT extern const size_t kReservedActionsLength;

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLoginOrLockScreenLength;

// Actions allowed while screen is locked (in addition to
// kActionsAllowedAtLoginOrLockScreen).
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLockScreenLength;

// Actions allowed while a modal window is up.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtModalWindow[];
ASH_EXPORT extern const size_t kActionsAllowedAtModalWindowLength;

// Actions which will not be repeated while holding an accelerator key.
ASH_EXPORT extern const AcceleratorAction kNonrepeatableActions[];
ASH_EXPORT extern const size_t kNonrepeatableActionsLength;

// Actions allowed in app mode.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedInAppMode[];
ASH_EXPORT extern const size_t kActionsAllowedInAppModeLength;

// Actions that require at least 1 window.
ASH_EXPORT extern const AcceleratorAction kActionsNeedingWindow[];
ASH_EXPORT extern const size_t kActionsNeedingWindowLength;

// Actions that can be performed while keeping the menu open.
ASH_EXPORT extern const AcceleratorAction kActionsKeepingMenuOpen[];
ASH_EXPORT extern const size_t kActionsKeepingMenuOpenLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
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
  ACCESSIBLE_FOCUS_NEXT,
  ACCESSIBLE_FOCUS_PREVIOUS,
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
  DEBUG_TOGGLE_DEVICE_SCALE_FACTOR,
  DEBUG_TOGGLE_SHOW_DEBUG_BORDERS,
  DEBUG_TOGGLE_SHOW_FPS_COUNTER,
  DEBUG_TOGGLE_SHOW_PAINT_RECTS,
  DISABLE_CAPS_LOCK,
  EXIT,
  FOCUS_NEXT_PANE,
  FOCUS_PREVIOUS_PANE,
  FOCUS_SHELF,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  LAUNCH_APP_0,
  LAUNCH_APP_1,
  LAUNCH_APP_2,
  LAUNCH_APP_3,
  LAUNCH_APP_4,
  LAUNCH_APP_5,
  LAUNCH_APP_6,
  LAUNCH_APP_7,
  LAUNCH_LAST_APP,
  LOCK_PRESSED,
  LOCK_RELEASED,
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
  POWER_PRESSED,
  POWER_RELEASED,
  PREVIOUS_IME,
  PRINT_LAYER_HIERARCHY,
  PRINT_UI_HIERARCHIES,
  PRINT_VIEW_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
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
  SILENCE_SPOKEN_FEEDBACK,
  SWAP_PRIMARY_DISPLAY,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_CAPS_LOCK,
  TOGGLE_CAPS_LOCK_BY_ALT_LWIN,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_FULLSCREEN,
  TOGGLE_MAXIMIZED,
  TOGGLE_OVERVIEW,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_TOUCH_VIEW_TESTING,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  TOUCH_HUD_MODE_CHANGE,
  TOUCH_HUD_PROJECTION_TOGGLE,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  WINDOW_MINIMIZE,
  WINDOW_POSITION_CENTER,
  WINDOW_SNAP_LEFT,
  WINDOW_SNAP_RIGHT,
#if defined(OS_CHROMEOS)
  ADD_REMOVE_DISPLAY,
  TOGGLE_MIRROR_MODE,
  DISABLE_GPU_WATCHDOG,
  LOCK_SCREEN,
  OPEN_CROSH,
  OPEN_FILE_MANAGER,
  SWITCH_TO_NEXT_USER,
  SWITCH_TO_PREVIOUS_USER,
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

// Accelerators handled by AcceleratorController.
ASH_EXPORT extern const AcceleratorData kAcceleratorData[];
ASH_EXPORT extern const size_t kAcceleratorDataLength;

#if !defined(NDEBUG)
// Accelerators useful when running on desktop. Debug build only.
ASH_EXPORT extern const AcceleratorData kDesktopAcceleratorData[];
ASH_EXPORT extern const size_t kDesktopAcceleratorDataLength;
#endif

// Debug accelerators enabled only when "Debugging keyboard shortcuts" flag
// (--ash-debug-shortcuts) is enabled.
ASH_EXPORT extern const AcceleratorData kDebugAcceleratorData[];
ASH_EXPORT extern const size_t kDebugAcceleratorDataLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT extern const AcceleratorAction kPreferredActions[];
ASH_EXPORT extern const size_t kPreferredActionsLength;

// Actions that are always handled in Ash.
ASH_EXPORT extern const AcceleratorAction kReservedActions[];
ASH_EXPORT extern const size_t kReservedActionsLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen, these actions are only handled if
// DebugShortcutsEnabled is true (command line switch 'ash-debug-shortcuts').
ASH_EXPORT extern const AcceleratorAction kReservedDebugActions[];
ASH_EXPORT extern const size_t kReservedDebugActionsLength;

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

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

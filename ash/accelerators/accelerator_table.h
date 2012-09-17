// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include "ash/ash_export.h"
#include "ui/base/events/event_constants.h"

namespace ash {

// Please put if/def sections at the end of the bare section and keep the list
// within each section in alphabetical order.
enum AcceleratorAction {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD_LINEAR,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_LINEAR,
  CYCLE_FORWARD_MRU,
  DISABLE_CAPS_LOCK,
  DISPLAY_TOGGLE_SCALE,
  EXIT,
  FOCUS_LAUNCHER,
  FOCUS_NEXT_PANE,
  FOCUS_PREVIOUS_PANE,
  FOCUS_SYSTEM_TRAY,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
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
  POWER_PRESSED,
  POWER_RELEASED,
  RESTORE_TAB,
  ROTATE_SCREEN,
  ROTATE_WINDOWS,
  SELECT_LAST_WIN,
  SELECT_WIN_0,
  SELECT_WIN_1,
  SELECT_WIN_2,
  SELECT_WIN_3,
  SELECT_WIN_4,
  SELECT_WIN_5,
  SELECT_WIN_6,
  SELECT_WIN_7,
  SHOW_KEYBOARD_OVERLAY,
  SHOW_OAK,
  SHOW_TASK_MANAGER,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,  // Control+F5
  TAKE_SCREENSHOT_BY_PRTSCN_KEY,  // PrtScn
  TOGGLE_APP_LIST,
  TOGGLE_CAPS_LOCK,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_MAXIMIZED,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_WIFI,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  WINDOW_MINIMIZE,
  WINDOW_POSITION_CENTER,
  WINDOW_SNAP_LEFT,
  WINDOW_SNAP_RIGHT,
#if defined(OS_CHROMEOS)
  CYCLE_DISPLAY_MODE,
  LOCK_SCREEN,
  OPEN_CROSH,
  OPEN_FILE_MANAGER_DIALOG,
  OPEN_FILE_MANAGER_TAB,
#endif
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_VIEW_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
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

// The number of elements in kAcceleratorData.
ASH_EXPORT extern const size_t kAcceleratorDataLength;

// Debug accelerators enabled only when "Debugging keyboard shortcuts" flag
// (--ash-debug-shortcuts) is enabled.
ASH_EXPORT extern const AcceleratorData kDebugAcceleratorData[];

// The number of elements in kDebugAcceleratorData.
ASH_EXPORT extern const size_t kDebugAcceleratorDataLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT extern const AcceleratorAction kReservedActions[];

// The number of elements in kReservedActions.
ASH_EXPORT extern const size_t kReservedActionsLength;

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[];

// The number of elements in kActionsAllowedAtLoginOrLockScreen.
ASH_EXPORT extern const size_t kActionsAllowedAtLoginOrLockScreenLength;

// Actions allowed while screen is locked (in addition to
// kActionsAllowedAtLoginOrLockScreen).
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLockScreen[];

// The number of elements in kActionsAllowedAtLockScreen.
ASH_EXPORT extern const size_t kActionsAllowedAtLockScreenLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

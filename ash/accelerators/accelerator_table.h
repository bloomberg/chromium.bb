// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#pragma once

#include "ash/ash_export.h"
#include "ui/aura/event.h"

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
  EXIT,
  FOCUS_LAUNCHER,
  FOCUS_TRAY,
  NEW_INCOGNITO_WINDOW,
  NEW_WINDOW,
  NEXT_IME,
  PREVIOUS_IME,
  ROTATE_WINDOWS,
  SEARCH_KEY,
  SELECT_LAST_WIN,
  SELECT_WIN_0,
  SELECT_WIN_1,
  SELECT_WIN_2,
  SELECT_WIN_3,
  SELECT_WIN_4,
  SELECT_WIN_5,
  SELECT_WIN_6,
  SELECT_WIN_7,
  SHOW_OAK,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_CAPS_LOCK,
  TOGGLE_SPOKEN_FEEDBACK,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  WINDOW_MAXIMIZE_RESTORE,
  WINDOW_MINIMIZE,
  WINDOW_POSITION_CENTER,
  WINDOW_SNAP_LEFT,
  WINDOW_SNAP_RIGHT,
#if defined(OS_CHROMEOS)
  LOCK_SCREEN,
  OPEN_CROSH,
  OPEN_FILE_MANAGER,
#endif
#if !defined(NDEBUG)
  MONITOR_ADD_REMOVE,
  MONITOR_CYCLE,
  MONITOR_TOGGLE_SCALE,
  PRINT_LAYER_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
#endif
};

struct AcceleratorData {
  bool trigger_on_press;
  ui::KeyboardCode keycode;
  bool shift;
  bool ctrl;
  bool alt;
  AcceleratorAction action;
};

// Accelerators handled by AcceleratorController.
ASH_EXPORT extern const AcceleratorData kAcceleratorData[];

// The number of elements in kAcceleratorData.
ASH_EXPORT extern const size_t kAcceleratorDataLength;

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLoginScreen[];

// The number of elements in kActionsAllowedAtLoginScreen.
ASH_EXPORT extern const size_t kActionsAllowedAtLoginScreenLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

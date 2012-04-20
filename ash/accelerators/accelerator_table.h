// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#pragma once

#include "ui/aura/event.h"

namespace ash {

enum AcceleratorAction {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
  CYCLE_BACKWARD_LINEAR,
  CYCLE_FORWARD_LINEAR,
  EXIT,
  NEW_INCOGNITO_WINDOW,
  NEW_WINDOW,
  NEXT_IME,
  PREVIOUS_IME,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  SEARCH_KEY,
  TOGGLE_CAPS_LOCK,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  SHOW_OAK,
  FOCUS_TRAY,
#if defined(OS_CHROMEOS)
  LOCK_SCREEN,
  OPEN_FILE_MANAGER,
  OPEN_CROSH,
#endif
  SELECT_WIN_0,
  SELECT_WIN_1,
  SELECT_WIN_2,
  SELECT_WIN_3,
  SELECT_WIN_4,
  SELECT_WIN_5,
  SELECT_WIN_6,
  SELECT_WIN_7,
  SELECT_LAST_WIN,
  ROTATE_WINDOWS,
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
  ADD_REMOVE_MONITOR,
  CYCLE_MONITOR,
#endif
};

struct AcceleratorData {
  ui::EventType type;
  ui::KeyboardCode keycode;
  bool shift;
  bool ctrl;
  bool alt;
  AcceleratorAction action;
};

// Accelerators handled by AcceleratorController.
extern const AcceleratorData kAcceleratorData[];

// The number of elements in kAcceleratorData.
extern const size_t kAcceleratorDataLength;

// Actions allowed while user is not signed in or screen is locked.
extern const AcceleratorAction kActionsAllowedAtLoginScreen[];

// The number of elements in kActionsAllowedAtLoginScreen.
extern const size_t kActionsAllowedAtLoginScreenLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

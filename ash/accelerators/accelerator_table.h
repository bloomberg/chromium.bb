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
  CYCLE_BACKWARD,
  CYCLE_FORWARD,
  EXIT,
  NEXT_IME,
  PREVIOUS_IME,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_CAPS_LOCK,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  SHOW_OAK,
#if defined(OS_CHROMEOS)
  LOCK_SCREEN,
#endif
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
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

// The numbers of elements in kAcceleratorData.
extern const size_t kAcceleratorDataLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

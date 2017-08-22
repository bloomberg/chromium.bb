// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "base/macros.h"

namespace ash {

// A class which controls keyboard brightness when Alt+F6, Alt+F7 or a
// multimedia key for keyboard brightness is pressed.
class ASH_EXPORT KeyboardBrightnessController
    : public KeyboardBrightnessControlDelegate {
 public:
  KeyboardBrightnessController();
  ~KeyboardBrightnessController() override;

 private:
  // Overridden from KeyboardBrightnessControlDelegate:
  void HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) override;
  void HandleKeyboardBrightnessUp(const ui::Accelerator& accelerator) override;

  DISALLOW_COPY_AND_ASSIGN(KeyboardBrightnessController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BRIGHTNESS_CONTROLLER_H_

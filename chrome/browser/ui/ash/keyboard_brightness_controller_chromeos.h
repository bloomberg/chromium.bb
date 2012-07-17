// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

#include "ash/system/keyboard_brightness/keyboard_brightness_control_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

// A class which controls keyboard brightness when Alt+F6, Alt+F7 or a
// multimedia key for keyboard brightness is pressed.
class KeyboardBrightnessController
    : public ash::KeyboardBrightnessControlDelegate {
 public:
  KeyboardBrightnessController() {}
  virtual ~KeyboardBrightnessController() {}

 private:
  // Overridden from ash::KeyboardBrightnessControlDelegate:
  virtual bool HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleKeyboardBrightnessUp(
      const ui::Accelerator& accelerator) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardBrightnessController);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

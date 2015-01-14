// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_OZONE_H_
#define ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_OZONE_H_

#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "base/macros.h"

namespace ash {

// Disables the internal mouse and keyboard for the duration of the class'
// lifetime.
class ScopedDisableInternalMouseAndKeyboardOzone
    : public ScopedDisableInternalMouseAndKeyboard {
 public:
  ScopedDisableInternalMouseAndKeyboardOzone();
  ~ScopedDisableInternalMouseAndKeyboardOzone() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedDisableInternalMouseAndKeyboardOzone);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_SCOPED_DISABLE_INTERNAL_MOUSE_AND_KEYBOARD_OZONE_H_

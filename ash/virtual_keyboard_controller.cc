// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include "ash/shell.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

VirtualKeyboardController::VirtualKeyboardController() {
  Shell::GetInstance()->AddShellObserver(this);
}

VirtualKeyboardController::~VirtualKeyboardController() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

void VirtualKeyboardController::OnMaximizeModeStarted() {
  keyboard::SetTouchKeyboardEnabled(true);
  Shell::GetInstance()->CreateKeyboard();
}

void VirtualKeyboardController::OnMaximizeModeEnded() {
  keyboard::SetTouchKeyboardEnabled(false);
  if (!keyboard::IsKeyboardEnabled())
    Shell::GetInstance()->DeactivateKeyboard();
}

}  // namespace ash

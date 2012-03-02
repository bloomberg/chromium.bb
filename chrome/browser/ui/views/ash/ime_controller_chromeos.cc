// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/ime_controller_chromeos.h"

#include "chrome/browser/chromeos/input_method/input_method_manager.h"

bool ImeController::HandleNextIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  return manager->SwitchToNextInputMethod();
}

bool ImeController::HandlePreviousIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  return manager->SwitchToPreviousInputMethod();
}

bool ImeController::HandleSwitchIme(const ui::Accelerator& accelerator) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  return manager->SwitchInputMethod(accelerator);
}

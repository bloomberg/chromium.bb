// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_chromeos.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

bool ImeController::CanCycleIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return manager->GetActiveIMEState()->CanCycleInputMethod();
}

void ImeController::HandleNextIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  manager->GetActiveIMEState()->SwitchToNextInputMethod();
}

void ImeController::HandlePreviousIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  manager->GetActiveIMEState()->SwitchToPreviousInputMethod();
}

bool ImeController::CanSwitchIme(const ui::Accelerator& accelerator) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return manager->GetActiveIMEState()->CanSwitchInputMethod(accelerator);
}

void ImeController::HandleSwitchIme(const ui::Accelerator& accelerator) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  manager->GetActiveIMEState()->SwitchInputMethod(accelerator);
}

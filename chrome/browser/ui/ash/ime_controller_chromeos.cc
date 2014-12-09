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

ui::Accelerator ImeController::RemapAccelerator(
    const ui::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.key_code();
  // On French keyboards the user needs to press a number key in conjunction
  // with the shift key. To get the right accelerator from our static table
  // we modify the received accelerator to match this. See
  // http://crbug.com/129017 for more details.
  if (key < ui::VKEY_0 || key > ui::VKEY_9 || !UsingFrenchInputMethod())
    return accelerator;

  // We toggle the shift key to get the correct accelerator from our table.
  int remapped_modifiers = accelerator.modifiers() ^ ui::EF_SHIFT_DOWN;

  ui::Accelerator remapped_accelerator(key, remapped_modifiers);
  remapped_accelerator.set_type(accelerator.type());
  remapped_accelerator.set_is_repeat(accelerator.IsRepeat());
  return remapped_accelerator;
}

bool ImeController::UsingFrenchInputMethod() const {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  const chromeos::input_method::InputMethodDescriptor& descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  const std::string& layout = descriptor.id();
  return (layout == "xkb:fr::fra" || layout == "xkb:be::fra");
}

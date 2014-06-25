// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_chromeos.h"

#include "chromeos/ime/input_method_manager.h"
#include "ui/base/accelerators/accelerator.h"

void ImeController::HandleNextIme() {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  manager->SwitchToNextInputMethod();
}

bool ImeController::HandlePreviousIme(const ui::Accelerator& accelerator) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return manager->SwitchToPreviousInputMethod(accelerator);
}

bool ImeController::HandleSwitchIme(const ui::Accelerator& accelerator) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return manager->SwitchInputMethod(accelerator);
}

ui::Accelerator ImeController::RemapAccelerator(
    const ui::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.key_code();
  int modifiers = accelerator.modifiers();
  // On French keyboards the user needs to press a number key in conjunction
  // with the shift key. To get the right accelerator from our static table
  // we modify the received accelerator to match this.
  if (key >= ui::VKEY_0 && key <= ui::VKEY_9) {
    // A keyboard layout can get changed by the user, so we perform quickly
    // this cheap layout test.
    // See http://crbug.com/129017 for more details.
    if (UsingFrenchInputMethod()) {
      // We toggle the shift key to get the correct accelerator from our table.
      modifiers ^= ui::EF_SHIFT_DOWN;
    }
  }
  ui::Accelerator remapped_accelerator(key, modifiers);
  remapped_accelerator.set_type(accelerator.type());
  return remapped_accelerator;
}

bool ImeController::UsingFrenchInputMethod() const {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  const chromeos::input_method::InputMethodDescriptor& descriptor =
      manager->GetCurrentInputMethod();
  const std::string& layout = descriptor.id();
  return (layout == "xkb:fr::fra" || layout == "xkb:be::fra");
}

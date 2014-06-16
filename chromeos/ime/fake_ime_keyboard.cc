// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/fake_ime_keyboard.h"

namespace chromeos {
namespace input_method {

FakeImeKeyboard::FakeImeKeyboard()
    : set_current_keyboard_layout_by_name_count_(0),
      caps_lock_is_enabled_(false),
      auto_repeat_is_enabled_(false) {
}

FakeImeKeyboard::~FakeImeKeyboard() {
}

void FakeImeKeyboard::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeImeKeyboard::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeImeKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  ++set_current_keyboard_layout_by_name_count_;
  last_layout_ = layout_name;
  return true;
}

bool FakeImeKeyboard::ReapplyCurrentKeyboardLayout() {
  return true;
}

void FakeImeKeyboard::ReapplyCurrentModifierLockStatus() {
}

void FakeImeKeyboard::DisableNumLock() {
}

void FakeImeKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  bool old_state = caps_lock_is_enabled_;
  caps_lock_is_enabled_ = enable_caps_lock;
  if (old_state != enable_caps_lock) {
    FOR_EACH_OBSERVER(ImeKeyboard::Observer, observers_,
                      OnCapsLockChanged(enable_caps_lock));
  }
}

bool FakeImeKeyboard::CapsLockIsEnabled() {
  return caps_lock_is_enabled_;
}

bool FakeImeKeyboard::IsISOLevel5ShiftAvailable() const {
  return false;
}

bool FakeImeKeyboard::IsAltGrAvailable() const {
  return false;
}

bool FakeImeKeyboard::SetAutoRepeatEnabled(bool enabled) {
  auto_repeat_is_enabled_ = enabled;
  return true;
}

bool FakeImeKeyboard::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  last_auto_repeat_rate_ = rate;
  return true;
}

}  // namespace input_method
}  // namespace chromeos

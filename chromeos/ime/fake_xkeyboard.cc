// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/fake_xkeyboard.h"

namespace chromeos {
namespace input_method {

FakeXKeyboard::FakeXKeyboard()
    : set_current_keyboard_layout_by_name_count_(0),
      caps_lock_is_enabled_(false),
      auto_repeat_is_enabled_(false) {
}

void FakeXKeyboard::AddObserver(XKeyboard::Observer* observer) {
}

void FakeXKeyboard::RemoveObserver(XKeyboard::Observer* observer) {
}

bool FakeXKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  ++set_current_keyboard_layout_by_name_count_;
  last_layout_ = layout_name;
  return true;
}

bool FakeXKeyboard::ReapplyCurrentKeyboardLayout() {
  return true;
}

void FakeXKeyboard::ReapplyCurrentModifierLockStatus() {
}

void FakeXKeyboard::DisableNumLock() {
}

void FakeXKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  caps_lock_is_enabled_ = enable_caps_lock;
}

bool FakeXKeyboard::CapsLockIsEnabled() {
  return caps_lock_is_enabled_;
}

bool FakeXKeyboard::IsISOLevel5ShiftAvailable() const {
  return false;
}

bool FakeXKeyboard::IsAltGrAvailable() const {
  return false;
}

bool FakeXKeyboard::SetAutoRepeatEnabled(bool enabled) {
  auto_repeat_is_enabled_ = enabled;
  return true;
}

bool FakeXKeyboard::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  last_auto_repeat_rate_ = rate;
  return true;
}

}  // namespace input_method
}  // namespace chromeos

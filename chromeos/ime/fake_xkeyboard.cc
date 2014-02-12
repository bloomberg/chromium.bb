// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/fake_xkeyboard.h"

namespace chromeos {
namespace input_method {

FakeXKeyboard::FakeXKeyboard()
    : set_current_keyboard_layout_by_name_count_(0),
      caps_lock_is_enabled_(false),
      num_lock_is_enabled_(false),
      auto_repeat_is_enabled_(false) {
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

void FakeXKeyboard::SetLockedModifiers(ModifierLockStatus new_caps_lock_status,
                                       ModifierLockStatus new_num_lock_status) {
  if (new_caps_lock_status != kDontChange) {
    caps_lock_is_enabled_ =
        (new_caps_lock_status == kEnableLock) ? true : false;
  }
  if (new_num_lock_status != kDontChange)
    num_lock_is_enabled_ = (new_num_lock_status == kEnableLock) ? true : false;
}

void FakeXKeyboard::SetNumLockEnabled(bool enable_num_lock) {
  num_lock_is_enabled_ = enable_num_lock;
}

void FakeXKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  caps_lock_is_enabled_ = enable_caps_lock;
}

bool FakeXKeyboard::NumLockIsEnabled() {
  return num_lock_is_enabled_;
}

bool FakeXKeyboard::CapsLockIsEnabled() {
  return caps_lock_is_enabled_;
}

unsigned int FakeXKeyboard::GetNumLockMask() {
  return 1;
}

void FakeXKeyboard::GetLockedModifiers(bool* out_caps_lock_enabled,
                                       bool* out_num_lock_enabled) {
  *out_caps_lock_enabled = caps_lock_is_enabled_;
  *out_num_lock_enabled = num_lock_is_enabled_;
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"

namespace chromeos {
namespace input_method {

MockXKeyboard::MockXKeyboard()
    : set_current_keyboard_layout_by_name_count_(0),
      caps_lock_is_enabled_(false),
      num_lock_is_enabled_(false) {
}

bool MockXKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  ++set_current_keyboard_layout_by_name_count_;
  last_layout_ = layout_name;
  return true;
}

bool MockXKeyboard::ReapplyCurrentKeyboardLayout() {
  return true;
}

void MockXKeyboard::ReapplyCurrentModifierLockStatus() {
}

void MockXKeyboard::SetLockedModifiers(ModifierLockStatus new_caps_lock_status,
                                       ModifierLockStatus new_num_lock_status) {
  if (new_caps_lock_status != kDontChange) {
    caps_lock_is_enabled_ =
        (new_caps_lock_status == kEnableLock) ? true : false;
  }
  if (new_num_lock_status != kDontChange)
    num_lock_is_enabled_ = (new_num_lock_status == kEnableLock) ? true : false;
}

void MockXKeyboard::SetNumLockEnabled(bool enable_num_lock) {
  num_lock_is_enabled_ = enable_num_lock;
}

void MockXKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  caps_lock_is_enabled_ = enable_caps_lock;
}

bool MockXKeyboard::NumLockIsEnabled() {
  return num_lock_is_enabled_;
}

bool MockXKeyboard::CapsLockIsEnabled() {
  return caps_lock_is_enabled_;
}

unsigned int MockXKeyboard::GetNumLockMask() {
  return 1;
}

void MockXKeyboard::GetLockedModifiers(bool* out_caps_lock_enabled,
                                       bool* out_num_lock_enabled) {
  *out_caps_lock_enabled = caps_lock_is_enabled_;
  *out_num_lock_enabled = num_lock_is_enabled_;
}

}  // namespace input_method
}  // namespace chromeos

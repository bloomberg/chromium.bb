// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"

namespace chromeos {
namespace input_method {

MockXKeyboard::MockXKeyboard()
    : set_current_keyboard_layout_by_name_count_(0) {
}

bool MockXKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  ++set_current_keyboard_layout_by_name_count_;
  last_layout_ = layout_name;
  return true;
}

bool MockXKeyboard::RemapModifierKeys(const ModifierMap& modifier_map) {
  return true;
}

bool MockXKeyboard::ReapplyCurrentKeyboardLayout() {
  return true;
}

void MockXKeyboard::ReapplyCurrentModifierLockStatus() {
}

void MockXKeyboard::SetLockedModifiers(
    ModifierLockStatus new_caps_lock_status,
    ModifierLockStatus new_num_lock_status) {
}

void MockXKeyboard::SetNumLockEnabled(bool enable_num_lock) {
}

void MockXKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
}

bool MockXKeyboard::NumLockIsEnabled() {
  return true;
}

bool MockXKeyboard::CapsLockIsEnabled() {
  return false;
}

std::string MockXKeyboard::CreateFullXkbLayoutName(
      const std::string& layout_name,
      const ModifierMap& modifire_map) {
  return "";
}

unsigned int MockXKeyboard::GetNumLockMask() {
  return 1;
}

void MockXKeyboard::GetLockedModifiers(bool* out_caps_lock_enabled,
                                       bool* out_num_lock_enabled) {
}

}  // namespace input_method
}  // namespace chromeos

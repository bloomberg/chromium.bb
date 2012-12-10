// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_input_method_delegate.h"

namespace chromeos {
namespace input_method {

MockInputMethodDelegate::MockInputMethodDelegate()
    : active_locale_("en"),
      update_system_input_method_count_(0),
      update_user_input_method_count_(0) {
}

MockInputMethodDelegate::~MockInputMethodDelegate() {
}

void MockInputMethodDelegate::SetSystemInputMethod(
    const std::string& input_method) {
  ++update_system_input_method_count_;
  system_input_method_ = input_method;
}

void MockInputMethodDelegate::SetUserInputMethod(
    const std::string& input_method) {
  ++update_user_input_method_count_;
  user_input_method_ = input_method;
}

std::string MockInputMethodDelegate::GetHardwareKeyboardLayout() const {
  return hardware_keyboard_layout_;
}

std::string MockInputMethodDelegate::GetActiveLocale() const {
  return active_locale_;
}

}  // namespace input_method
}  // namespace chromeos

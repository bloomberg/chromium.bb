// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/mock_input_method_delegate.h"

namespace chromeos {
namespace input_method {

MockInputMethodDelegate::MockInputMethodDelegate()
    : active_locale_("en") {
}

MockInputMethodDelegate::~MockInputMethodDelegate() {
}

std::string MockInputMethodDelegate::GetHardwareKeyboardLayout() const {
  return hardware_keyboard_layout_;
}

std::string MockInputMethodDelegate::GetActiveLocale() const {
  return active_locale_;
}

}  // namespace input_method
}  // namespace chromeos

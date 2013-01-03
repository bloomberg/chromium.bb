// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate.h"

namespace chromeos {
namespace input_method {

class MockInputMethodDelegate : public InputMethodDelegate {
 public:
  MockInputMethodDelegate();
  virtual ~MockInputMethodDelegate();

  // InputMethodDelegate implementation:
  virtual std::string GetHardwareKeyboardLayout() const OVERRIDE;
  virtual std::string GetActiveLocale() const OVERRIDE;

  void set_hardware_keyboard_layout(const std::string& value) {
    hardware_keyboard_layout_ = value;
  }

  void set_active_locale(const std::string& value) {
    active_locale_ = value;
  }

 private:
  std::string hardware_keyboard_layout_;
  std::string active_locale_;
  DISALLOW_COPY_AND_ASSIGN(MockInputMethodDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_DELEGATE_H_

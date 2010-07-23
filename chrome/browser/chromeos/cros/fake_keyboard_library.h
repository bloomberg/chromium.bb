// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_KEYBOARD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_KEYBOARD_LIBRARY_H_

#include <string>

#include "chrome/browser/chromeos/cros/keyboard_library.h"

namespace chromeos {

class FakeKeyboardLibrary : public KeyboardLibrary {
 public:
  FakeKeyboardLibrary() {}
  virtual ~FakeKeyboardLibrary() {}
  virtual std::string GetCurrentKeyboardLayoutName() const {
    return "";
  }
  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) {
    return true;
  }
  virtual bool GetKeyboardLayoutPerWindow(bool* is_per_window) const {
    return true;
  }
  virtual bool SetKeyboardLayoutPerWindow(bool is_per_window) {
    return true;
  }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_KEYBOARD_LIBRARY_H_

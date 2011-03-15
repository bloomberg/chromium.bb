// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_KEYBOARD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_KEYBOARD_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockKeyboardLibrary : public KeyboardLibrary {
 public:
  MockKeyboardLibrary() {}
  virtual ~MockKeyboardLibrary() {}

  MOCK_METHOD1(SetCurrentKeyboardLayoutByName, bool(const std::string&));
  MOCK_METHOD1(RemapModifierKeys, bool(const ModifierMap&));
  MOCK_METHOD1(SetAutoRepeatEnabled, bool(bool));
  MOCK_METHOD1(SetAutoRepeatRate, bool(const AutoRepeatRate&));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_KEYBOARD_LIBRARY_H_

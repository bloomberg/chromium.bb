// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_keyboard_library.h"

namespace chromeos {

std::string FakeKeyboardLibrary::GetCurrentKeyboardLayoutName() const {
  return "";
}

bool FakeKeyboardLibrary::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  return true;
}

bool FakeKeyboardLibrary::GetKeyboardLayoutPerWindow(
    bool* is_per_window) const {
  return true;
}

bool FakeKeyboardLibrary::SetKeyboardLayoutPerWindow(bool is_per_window) {
  return true;
}

}  // namespace chromeos

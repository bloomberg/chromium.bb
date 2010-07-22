// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/keyboard_library.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "cros/chromeos_keyboard.h"

namespace chromeos {

std::string KeyboardLibraryImpl::GetCurrentKeyboardLayoutName() const {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::GetCurrentKeyboardLayoutName();
  }
  return "";
}

bool KeyboardLibraryImpl::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::SetCurrentKeyboardLayoutByName(layout_name);
  }
  return false;
}

bool KeyboardLibraryImpl::GetKeyboardLayoutPerWindow(
    bool* is_per_window) const {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::GetKeyboardLayoutPerWindow(is_per_window);
  }
  return false;
}

bool KeyboardLibraryImpl::SetKeyboardLayoutPerWindow(bool is_per_window) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::SetKeyboardLayoutPerWindow(is_per_window);
  }
  return false;
}

}  // namespace chromeos

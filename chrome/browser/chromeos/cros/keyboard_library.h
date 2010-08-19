// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_
#pragma once

#include "cros/chromeos_keyboard.h"

#include <string>

#include "base/basictypes.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS keyboard library APIs.
class KeyboardLibrary {
 public:
  virtual ~KeyboardLibrary() {}

  // Returns the hardware layout name like "xkb:us::eng". On error, returns "".
  virtual std::string GetHardwareKeyboardLayoutName() const = 0;

  // Returns the current layout name like "us". On error, returns "".
  virtual std::string GetCurrentKeyboardLayoutName() const = 0;

  // Sets the current keyboard layout to |layout_name|.  Returns true on
  // success.
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) = 0;

  // Remaps modifier keys.  Returns true on success.
  virtual bool RemapModifierKeys(const ModifierMap& modifier_map) = 0;

  // Gets whehter we have separate keyboard layout per window, or not. The
  // result is stored in |is_per_window|.  Returns true on success.
  virtual bool GetKeyboardLayoutPerWindow(bool* is_per_window) const = 0;

  // Sets whether we have separate keyboard layout per window, or not. If false
  // is given, the same keyboard layout will be shared for all applications.
  // Returns true on success.
  virtual bool SetKeyboardLayoutPerWindow(bool is_per_window) = 0;

  // Gets the current auto-repeat mode of the keyboard. The result is stored in
  // |enabled|. Returns true on success.
  virtual bool GetAutoRepeatEnabled(bool* enabled) const = 0;

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  virtual bool SetAutoRepeatEnabled(bool enabled) = 0;

  // Gets the current auto-repeat rate of the keyboard. The result is stored in
  // |out_rate|. Returns true on success.
  virtual bool GetAutoRepeatRate(AutoRepeatRate* out_rate) const = 0;

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success.
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static KeyboardLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_

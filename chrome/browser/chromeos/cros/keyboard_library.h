// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_

#include "cros/chromeos_keyboard.h"

#include <string>

#include "base/basictypes.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS keyboard library APIs.
class KeyboardLibrary {
 public:
  virtual ~KeyboardLibrary() {}

  // Returns the current layout name like "us". On error, returns "".
  virtual std::string GetCurrentKeyboardLayoutName() const = 0;

  // Sets the current keyboard layout to |layout_name|.  Returns true on
  // success.
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) = 0;

  // Gets whehter we have separate keyboard layout per window, or not. The
  // result is stored in |is_per_window|.  Returns true on success.
  virtual bool GetKeyboardLayoutPerWindow(bool* is_per_window) const = 0;

  // Sets whether we have separate keyboard layout per window, or not. If false
  // is given, the same keyboard layout will be shared for all applications.
  // Returns true on success.
  virtual bool SetKeyboardLayoutPerWindow(bool is_per_window) = 0;
};

class KeyboardLibraryImpl : public KeyboardLibrary {
 public:
  KeyboardLibraryImpl() {}
  virtual ~KeyboardLibraryImpl() {}

  // KeyboardLibrary overrides.
  virtual std::string GetCurrentKeyboardLayoutName() const;
  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name);
  virtual bool GetKeyboardLayoutPerWindow(bool* is_per_window) const;
  virtual bool SetKeyboardLayoutPerWindow(bool is_per_window);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_KEYBOARD_LIBRARY_H_

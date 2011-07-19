// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_KEYMAP_H_
#define CHROME_TEST_WEBDRIVER_KEYMAP_H_

#include <map>

#include "chrome/test/automation/window_proxy.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace webdriver {

// Maps the key space used by WebDriver to Chrome for Linux/Mac/Windows.
class KeyMap {
 public:
  KeyMap();
  ~KeyMap();
  ui::KeyboardCode Get(const wchar_t& key) const;

  bool Press(const scoped_refptr<WindowProxy>& window,
             const ui::KeyboardCode key_code,
             const wchar_t& key);

  // Sets the Shift, Alt, Cntl, and Cmd keys to not pressed.
  void ClearModifiers();

 private:
  bool shift_;
  bool alt_;
  bool control_;
  bool command_;
  std::map<wchar_t, ui::KeyboardCode> keys_;
  std::map<wchar_t, ui::KeyboardCode> shifted_keys_;
  DISALLOW_COPY_AND_ASSIGN(KeyMap);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_KEYMAP_H_


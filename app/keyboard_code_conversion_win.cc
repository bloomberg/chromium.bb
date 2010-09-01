// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/keyboard_code_conversion_win.h"

namespace app {

WORD WindowsKeyCodeForKeyboardCode(app::KeyboardCode keycode) {
  return static_cast<WORD>(keycode);
}

app::KeyboardCode KeyboardCodeForWindowsKeyCode(WORD keycode) {
  return static_cast<app::KeyboardCode>(keycode);
}

}  // namespace app

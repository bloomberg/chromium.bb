// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_KEYBOARD_CODE_CONVERSION_WIN_H_
#define APP_KEYBOARD_CODE_CONVERSION_WIN_H_
#pragma once

#include "app/keyboard_codes.h"

namespace app {

// Methods to convert app::KeyboardCode/Windows virtual key type methods.
WORD WindowsKeyCodeForKeyboardCode(app::KeyboardCode keycode);
app::KeyboardCode KeyboardCodeForWindowsKeyCode(WORD keycode);

}  // namespace app

#endif  // APP_KEYBOARD_CODE_CONVERSION_WIN_H_

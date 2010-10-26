// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_KEYBOARD_CODE_CONVERSION_X_H_
#define APP_KEYBOARD_CODE_CONVERSION_X_H_

#include "app/keyboard_codes_posix.h"

typedef union _XEvent XEvent;

namespace app {

KeyboardCode KeyboardCodeFromXKeyEvent(XEvent* xev);

}  // namespace app

#endif  // APP_KEYBOARD_CODE_CONVERSION_X_H_

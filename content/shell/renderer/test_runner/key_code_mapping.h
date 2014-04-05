// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_KEY_CODE_MAPPING_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_KEY_CODE_MAPPING_H_

namespace content {

// The keycodes match the values of the virtual keycodes found here
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
enum {
    VKEY_RETURN   = 0x0D,
    VKEY_ESCAPE   = 0x1B,
    VKEY_PRIOR    = 0x21,
    VKEY_NEXT     = 0x22,
    VKEY_END      = 0x23,
    VKEY_HOME     = 0x24,
    VKEY_LEFT     = 0x25,
    VKEY_UP       = 0x26,
    VKEY_RIGHT    = 0x27,
    VKEY_DOWN     = 0x28,
    VKEY_SNAPSHOT = 0x2C,
    VKEY_INSERT   = 0x2D,
    VKEY_DELETE   = 0x2E,
    VKEY_APPS     = 0x5D,
    VKEY_F1       = 0x70,
    VKEY_NUMLOCK  = 0x90,
    VKEY_LSHIFT   = 0xA0,
    VKEY_RSHIFT   = 0xA1,
    VKEY_LCONTROL = 0xA2,
    VKEY_RCONTROL = 0xA3,
    VKEY_LMENU    = 0xA4,
    VKEY_RMENU    = 0xA5,
};

// Map a windows keycode to a native keycode on defined(__linux__) && defined(TOOLKIT_GTK).
int NativeKeyCodeForWindowsKeyCode(int keysym);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_KEY_CODE_MAPPING_H_

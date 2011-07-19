// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_SIMULATE_INPUT_H_
#define CHROME_FRAME_TEST_SIMULATE_INPUT_H_

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "base/process_util.h"

namespace simulate_input {

enum Modifier {
  NONE,
  SHIFT = 1,
  CONTROL = 2,
  ALT = 4
};

// Bring a window into foreground to receive user input.
// Note that this may not work on
bool ForceSetForegroundWindow(HWND window);

// Looks for a top level window owned by the given process id and
// calls ForceSetForegroundWindow on it.
bool EnsureProcessInForeground(base::ProcessId process_id);

// Helper function to set keyboard focus to a window. This is achieved by
// sending a mouse move followed by a mouse down/mouse up combination to the
// window.
void SetKeyboardFocusToWindow(HWND window);

// Sends a keystroke to the currently active application with optional
// modifiers set.
void SendMnemonic(WORD mnemonic_char, Modifier modifiers, bool extended,
                  bool unicode);

// Sends a mouse click to the desktop.
enum MouseButton { LEFT, RIGHT, MIDDLE, X };
void SendMouseClick(int x, int y, MouseButton button);
// Sends a mouse click to the window passed in, after ensuring that the window
// is in the foreground.
void SendMouseClick(HWND window, int x, int y, MouseButton button);

// Translates a single char to a virtual key.
void SendScanCode(short scan_code, Modifier modifiers);
void SendCharA(char c, Modifier modifiers);
void SendCharW(wchar_t c, Modifier modifiers);

// Sends extended keystroke to the currently active application with optional
// modifiers set.
void SendExtendedKey(WORD key, Modifier modifiers);

// Iterates through all the characters in the string and simulates
// keyboard input.  The input goes to the currently active application.
void SendStringW(const std::wstring& s);
void SendStringA(const std::string& s);

}  // end namespace simulate_input

#endif  // CHROME_FRAME_TEST_SIMULATE_INPUT_H_


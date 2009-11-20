// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/process_util.h"

namespace chrome_frame_test {

bool IsTopLevelWindow(HWND window);
int CloseVisibleWindowsOnAllThreads(HANDLE process);
bool ForceSetForegroundWindow(HWND window);
bool EnsureProcessInForeground(base::ProcessId process_id);

// Iterates through all the characters in the string and simulates
// keyboard input.  The input goes to the currently active application.
bool SendString(const wchar_t* s);

// Sends a virtual key such as VK_TAB, VK_RETURN or a character that has been
// translated to a virtual key.
// The extended flag indicates if this is an extended key
void SendVirtualKey(int16 key, bool extended);

// Translates a single char to a virtual key and calls SendVirtualKey.
void SendChar(char c);

// Sends an ascii string, char by char (calls SendChar for each).
void SendString(const char* s);

// Sends a keystroke to the currently active application with optional
// modifiers set.
bool SendMnemonic(WORD mnemonic_char, bool shift_pressed, bool control_pressed,
                  bool alt_pressed);

base::ProcessHandle LaunchFirefox(const std::wstring& url);
base::ProcessHandle LaunchOpera(const std::wstring& url);
base::ProcessHandle LaunchIE(const std::wstring& url);
base::ProcessHandle LaunchSafari(const std::wstring& url);
base::ProcessHandle LaunchChrome(const std::wstring& url);

// Attempts to close all open IE windows.
// The return value is the number of windows closed.
// @note: this function requires COM to be initialized on the calling thread.
// Since the caller might be running in either MTA or STA, the function does
// not perform this initialization itself.
int CloseAllIEWindows();

extern const wchar_t kIEImageName[];
extern const wchar_t kIEBrokerImageName[];
extern const wchar_t kFirefoxImageName[];
extern const wchar_t kOperaImageName[];
extern const wchar_t kSafariImageName[];
extern const wchar_t kChromeImageName[];

// Displays the chrome frame context menu by posting mouse move messages to
// Chrome
void ShowChromeFrameContextMenu();

// Sends keyboard messages to the chrome frame context menu to select the About
// Chrome frame option.
void SelectAboutChromeFrame();

// Returns a handle to the chrome frame render widget child window.
// Returns NULL on failure.
HWND GetChromeRendererWindow();

// Sends the specified input to the window passed in.
void SendInputToWindow(HWND window, const std::string& input_string);

// Helper function to set keyboard focus to a window. This is achieved by
// sending a mouse move followed by a mouse down/mouse up combination to the
// window.
void SetKeyboardFocusToWindow(HWND window, int x, int y);

// Temporarily impersonate the current thread to low integrity for the lifetime
// of the object. Destructor will automatically revert integrity level.
class LowIntegrityToken {
 public:
  LowIntegrityToken();
  ~LowIntegrityToken();
  BOOL Impersonate();
  BOOL RevertToSelf();
 protected:
  static bool IsImpersonated();
  bool impersonated_;
};

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_

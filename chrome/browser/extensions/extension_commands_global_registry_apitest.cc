// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/base_window.h"

#if defined(OS_LINUX)
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace extensions {

typedef ExtensionApiTest GlobalCommandsApiTest;

#if defined(OS_LINUX)
// Send a simulated key press and release event, where |control|, |shift| or
// |alt| indicates whether the key is struck with corresponding modifier.
void SendNativeKeyEventToXDisplay(ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt) {
  Display* display = gfx::GetXDisplay();
  KeyCode ctrl_key_code = XKeysymToKeycode(display, XK_Control_L);
  KeyCode shift_key_code = XKeysymToKeycode(display, XK_Shift_L);
  KeyCode alt_key_code = XKeysymToKeycode(display, XK_Alt_L);

  // Release modifiers first of all to make sure this function can work as
  // expected. For example, when |control| is false, but the status of Ctrl key
  // is down, we will generate a keyboard event with unwanted Ctrl key.
  XTestFakeKeyEvent(display, ctrl_key_code, False, CurrentTime);
  XTestFakeKeyEvent(display, shift_key_code, False, CurrentTime);
  XTestFakeKeyEvent(display, alt_key_code, False, CurrentTime);

  typedef std::vector<KeyCode> KeyCodes;
  KeyCodes key_codes;
  if (control)
    key_codes.push_back(ctrl_key_code);
  if (shift)
    key_codes.push_back(shift_key_code);
  if (alt)
    key_codes.push_back(alt_key_code);

  key_codes.push_back(XKeysymToKeycode(display,
                                       XKeysymForWindowsKeyCode(key, false)));

  // Simulate the keys being pressed.
  for (KeyCodes::iterator it = key_codes.begin(); it != key_codes.end(); it++)
    XTestFakeKeyEvent(display, *it, True, CurrentTime);

  // Simulate the keys being released.
  for (KeyCodes::iterator it = key_codes.begin(); it != key_codes.end(); it++)
    XTestFakeKeyEvent(display, *it, False, CurrentTime);

  XFlush(display);
}
#endif  // OS_LINUX

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// The feature is only fully implemented on Windows and Linux, other platforms
// coming.
#define MAYBE_GlobalCommand GlobalCommand
#else
#define MAYBE_GlobalCommand DISABLED_GlobalCommand
#endif

// Test the basics of global commands and make sure they work when Chrome
// doesn't have focus. Also test that non-global commands are not treated as
// global and that keys beyond Ctrl+Shift+[0..9] cannot be auto-assigned by an
// extension.
IN_PROC_BROWSER_TEST_F(GlobalCommandsApiTest, MAYBE_GlobalCommand) {
  FeatureSwitch::ScopedOverride enable_global_commands(
      FeatureSwitch::global_commands(), true);

  // Load the extension in the non-incognito browser.
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/global")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

#if !defined(OS_LINUX)
  // Our infrastructure for sending keys expects a browser to send them to, but
  // to properly test global shortcuts you need to send them to another target.
  // So, create an incognito browser to use as a target to send the shortcuts
  // to. It will ignore all of them and allow us test whether the global
  // shortcut really is global in nature and also that the non-global shortcut
  // is non-global.
  Browser* incognito_browser = CreateIncognitoBrowser();

  // Try to activate the non-global shortcut (Ctrl+Shift+1) and the
  // non-assignable shortcut (Ctrl+Shift+A) by sending the keystrokes to the
  // incognito browser. Both shortcuts should have no effect (extension is not
  // loaded there).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_1, true, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_A, true, true, false, false));

  // Activate the shortcut (Ctrl+Shift+9). This should have an effect.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_9, true, true, false, false));
#else
  // Create an incognito browser to capture the focus.
  CreateIncognitoBrowser();

  // On Linux, our infrastructure for sending keys just synthesize keyboard
  // event and send them directly to the specified window, without notifying the
  // X root window. It didn't work while testing global shortcut because the
  // stuff of global shortcut on Linux need to be notified when KeyPress event
  // is happening on X root window. So we simulate the keyboard input here.
  SendNativeKeyEventToXDisplay(ui::VKEY_1, true, true, false);
  SendNativeKeyEventToXDisplay(ui::VKEY_A, true, true, false);
  SendNativeKeyEventToXDisplay(ui::VKEY_9, true, true, false);
#endif

  // If this fails, it might be because the global shortcut failed to work,
  // but it might also be because the non-global shortcuts unexpectedly
  // worked.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions

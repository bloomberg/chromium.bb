// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

#include "base/basictypes.h"

namespace ui {
class KeyEvent;
}

namespace chromeos {

// KeyboardDrivenEventRewriter removes the modifier flags from
// Ctrl+Alt+Shift+<Arrow keys|Enter> key events. This mapping only happens
// on login screen and only when the keyboard driven oobe flag is enabled in the
// OEM manifest.
class KeyboardDrivenEventRewriter {
 public:
  KeyboardDrivenEventRewriter();
  ~KeyboardDrivenEventRewriter();

  // Calls RewriteEvent to modify |event| if it is on login screen and the
  // keyboard driven flag is enabled.
  void RewriteIfKeyboardDrivenOnLoginScreen(ui::KeyEvent* event);

  // Calls RewriteEvent for testing.
  void RewriteForTesting(ui::KeyEvent* event);

 private:
  void RewriteEvent(ui::KeyEvent* event);

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenEventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

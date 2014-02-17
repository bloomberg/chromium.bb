// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

#include "base/basictypes.h"

typedef union _XEvent XEvent;

namespace chromeos {

// KeyboardDrivenEventRewriter removes the modifier flags from
// Shift+<Arrow keys|Enter|F6> key events. This mapping only happens
// on login screen and only when the keyboard driven oobe is enabled.
class KeyboardDrivenEventRewriter {
 public:
  KeyboardDrivenEventRewriter();
  ~KeyboardDrivenEventRewriter();

  // Calls RewriteEvent to modify |event| if it is on login screen and the
  // keyboard driven flag is enabled. Returns true if the event is changed
  // and no further changes should happen.
  bool RewriteIfKeyboardDrivenOnLoginScreen(XEvent* event);

  // Calls RewriteEvent for testing.
  bool RewriteForTesting(XEvent* event);

 private:
  bool RewriteEvent(XEvent* event);

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenEventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

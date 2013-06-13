// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CAPS_LOCK_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_CAPS_LOCK_REWRITER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace ui {
class KeyEvent;
}

namespace chromeos {

namespace input_method {
class XKeyboard;
}

// CapsLockRewriter implements a smart/special caps lock triggered by a special
// key. The caps lock logic works like the following:
// - The trigger key turns on caps lock for the next input char and
//   automatically clears the caps lock after the next input char.
// - The caps lock remains on when user inputs with the trigger key holding
//   down and automatically turns off when the trigger key is released. This
//   behavior is similar to the SHIFT key.
// - The caps lock remains on if the trigger key is released after a certain
//   time (i.e. long pressing the trigger key) or if the trigger key is pressed
//   again within a certain time (i.e. double pressing the trigger key).
class CapsLockRewriter {
 public:
  // The default minimum duration in milliseconds to hold down the trigger key
  // to be considered as a long press.
  static const int kDefaultLongPressDurationInMs = 1000;

  // The default maximum duration in milliseconds between two trigger key press
  // events to be considered as a double press.
  static const int kDefaultDoublePressDurationInMs = 250;

  CapsLockRewriter();
  ~CapsLockRewriter();

  // Process incoming keyboard events.
  void RewriteEvent(ui::KeyEvent* event, const base::TimeTicks& now);

  ui::KeyboardCode trigger_key() const { return trigger_key_; }

  void set_xkeyboard_for_testing(input_method::XKeyboard* xkeyboad) {
    xkeyboard_for_testing_ = xkeyboad;
  }

 private:
  enum State {
    NONE,       // Normal, caps lock is not turned on by this class.
    TRANSIENT,  // Caps lock is on for the next input.
    ON,         // Caps lock stays on until the trigger key is pressed again.
  };

  State state_;

  // An XKeyboard to be used in test. In normal case, it is set to NULL and
  // the XKeyboard instance in input method manager is used.
  input_method::XKeyboard* xkeyboard_for_testing_;

  // The key code of the caps lock trigger key.
  ui::KeyboardCode trigger_key_;

  // True if trigger key released event is NOT received since TRANSIENT/ON state
  // starts.
  bool trigger_key_down_;

  // The time when the trigger key is pressed in NONE state. This is used to
  // determine the long press and double press case.
  base::TimeTicks trigger_key_down_tick_;

  // True if key events other than the trigger key have been received since
  // TRANSIENT/ON state starts.
  bool saw_other_key_while_trigger_key_down_;

  // The minimum duration to hold the trigger key to be considered as a long
  // press.
  const base::TimeDelta long_press_duration_;

  // The maximum duration between two trigger key press events to be considered
  // as a double press.
  const base::TimeDelta double_press_duration_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CAPS_LOCK_REWRITER_H_

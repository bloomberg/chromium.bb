// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_STUCK_BUTTON_INHIBITOR_FILTER_INTERPRETER_H_
#define GESTURES_STUCK_BUTTON_INHIBITOR_FILTER_INTERPRETER_H_

// This class monitors the input and output button state and finger count
// on the touchpad. It make sure that if all fingers have left the touchpad
// and the physical button is up, that we don't get in a situation where we've
// sent a button-down gesture, but never sent a button-up gesture. If that
// were to happen, the user would be in the unfortunate situation of having
// the mouse button seemingly stuck down.

namespace gestures {

class StuckButtonInhibitorFilterInterpreter : public Interpreter {
 public:
  // Takes ownership of |next|:
  explicit StuckButtonInhibitorFilterInterpreter(Interpreter* next);
  virtual ~StuckButtonInhibitorFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  void HandleHardwareState(const HardwareState& hwstate);
  void HandleGesture(Gesture** gesture, stime_t next_timeout, stime_t* timeout);

  scoped_ptr<Interpreter> next_;

  bool incoming_button_must_be_up_;

  // these buttons have been reported as down via a gesture:
  unsigned sent_buttons_down_;

  bool next_expects_timer_;  // True if next_ is expecting a call to HandleTimer

  Gesture result_;  // For when we return a button up
};

}  // namespace gestures

#endif  // GESTURES_STUCK_BUTTON_INHIBITOR_FILTER_INTERPRETER_H_

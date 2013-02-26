// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_MOUSE_INTERPRETER_H_
#define GESTURES_MOUSE_INTERPRETER_H_

namespace gestures {

class MouseInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(MouseInterpreterTest, SimpleTest);
 public:
  MouseInterpreter(PropRegistry* prop_reg, Tracer* tracer);
  virtual ~MouseInterpreter() {};

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

 private:
  HardwareState prev_state_;
  Gesture result_;
};

// This function interprets mouse events, which include button clicking,
// scroll wheel movement, and mouse movement.  This function needs two
// consecutive HardwareState.  If no mouse events are presented, result
// object is not modified.
void InterpretMouseEvent(const HardwareState& prev_state,
                         const HardwareState& hwstate,
                         Gesture* result);

}  // namespace gestures

#endif  // GESTURES_MOUSE_INTERPRETER_H_

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_MULTITOUCH_MOUSE_INTERPRETER_H_
#define GESTURES_MULTITOUCH_MOUSE_INTERPRETER_H_

namespace gestures {

class MultitouchMouseInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(MultitouchMouseInterpreterTest, SimpleTest);
 public:
  MultitouchMouseInterpreter(PropRegistry* prop_reg, Tracer* tracer);
  virtual ~MultitouchMouseInterpreter() {}

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

  void SetHardwarePropertiesImpl(const HardwareProperties& hw_props);

 private:
  void InterpretMultitouchEvent(Gesture* result);

  HardwareStateBuffer state_buffer_;
  ScrollEventBuffer scroll_buffer_;

  HardwareProperties hw_props_;

  FingerMap prev_gs_fingers_;
  FingerMap gs_fingers_;

  GestureType prev_gesture_type_;
  GestureType current_gesture_type_;

  Gesture prev_result_;
  Gesture result_;

  ScrollManager scroll_manager_;
};

}  // namespace gestures

#endif  // GESTURES_MULTITOUCH_MOUSE_INTERPRETER_H_

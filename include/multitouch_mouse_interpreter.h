// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/gestures.h"
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
  virtual ~MultitouchMouseInterpreter();

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

  void SetHardwarePropertiesImpl(const HardwareProperties& hw_props);

 private:
  void InterpretMultitouchEvent(Gesture* result);

  // states_ is a circular buffer of HardwareState that is index relative to
  // the current HardwareState; that is, the current one is at index 0, the
  // previous one is at index -1, and so on.
  HardwareState states_[2];
  int current_state_pos_;
  void PushState(const HardwareState& state);
  void ResetStates(size_t max_finger_cnt);
  // Index 0 is the current HardwareState, -1 is the previous HardwareState,
  // and so on.
  const HardwareState& State(int index) const;

  HardwareProperties hw_props_;
  Gesture result_;
};

}  // namespace gestures

#endif  // GESTURES_MULTITOUCH_MOUSE_INTERPRETER_H_

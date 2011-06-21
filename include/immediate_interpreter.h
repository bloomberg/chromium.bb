// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_IMMEDIATE_INTERPRETER_H_
#define GESTURES_IMMEDIATE_INTERPRETER_H_

namespace gestures {

// This interpreter keeps some memory of the past and, for each incoming
// frame of hardware state, immediately determines the gestures to the best
// of its abilities.

// Currently it simply does very basic pointer movement.

class ImmediateInterpreter : public Interpreter {
 public:
  ImmediateInterpreter();
  virtual ~ImmediateInterpreter();
  ustime_t Push(const HardwareState* hwstate);
  const SpeculativeGestures* Back(ustime_t now) const { return &result_; }
  ustime_t Pop(ustime_t now) { return 0; }

  void SetHardwareProperties(const HardwareProperties* hw_props);

 private:
  // Does a deep copy of hwstate into prev_state_
  void SetPrevState(const HardwareState* hwstate);

  HardwareState prev_state_;
  HardwareProperties hw_props_;
  SpeculativeGestures result_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_

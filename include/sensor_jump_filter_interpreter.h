// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/map.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_SENSOR_JUMP_FILTER_INTERPRETER_H_
#define GESTURES_SENSOR_JUMP_FILTER_INTERPRETER_H_

namespace gestures {

// This filter interpreter looks for fingers that suspiciously jump position,
// something that can occur on some touchpads when a finger is large and
// spanning multiple sensors.

// When a suspicious jump is detected, the finger is flagged with a warp
// flag for the axis in which the jump occurred.

class SensorJumpFilterInterpreter : public Interpreter,
                                    public PropertyDelegate {
  FRIEND_TEST(SensorJumpFilterInterpreterTest, SimpleTest);
  FRIEND_TEST(SensorJumpFilterInterpreterTest, ActualLogTest);
 public:
  // Takes ownership of |next|:
  SensorJumpFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~SensorJumpFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  scoped_ptr<Interpreter> next_;

  // Whether or not this filter is enabled. If disabled, it behaves as a
  // simple passthrough.
  BoolProperty enabled_;
  // The smallest distance that may be considered for warp.
  DoubleProperty min_warp_dist_;
  // The largest distance that may be considered for warp.
  DoubleProperty max_warp_dist_;
  // A suspiciously sized jump may be tolerated if it's length falls within
  // similar_multiplier_ * the previous frame's movement.
  DoubleProperty similar_multiplier_;

  // Fingers from the previous two SyncInterpret calls. previous_input_[0]
  // is the more recent.
  map<short, FingerState, kMaxFingers> previous_input_[2];

  // When a finger is flagged with a warp flag for the first time, we note it
  // here. first_flag_[0] is the X axis; first_flag_[1] is Y.
  set<short, kMaxFingers> first_flag_[2];
};

}  // namespace gestures

#endif  // GESTURES_SENSOR_JUMP_FILTER_INTERPRETER_H_

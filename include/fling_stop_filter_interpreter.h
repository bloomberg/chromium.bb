// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_FLING_STOP_FILTER_INTERPRETER_H_
#define GESTURES_FLING_STOP_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter generates the fling-stop messages when new fingers
// arrive on the pad.

class FlingStopFilterInterpreter : public Interpreter {
  FRIEND_TEST(FlingStopFilterInterpreterTest, SimpleTest);
 public:
  // Takes ownership of |next|:
  FlingStopFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~FlingStopFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
   // May override an outgoing gesture with a fling stop gesture.
  void EvaluateFlingStop(bool finger_added, stime_t now, Gesture* result);

  scoped_ptr<Interpreter> next_;

  // finger_cnt from previously input HardwareState.
  short prev_finger_cnt_;
  // timestamp from previous input HardwareState.
  stime_t prev_timestamp_;

  // Result to pass out.
  Gesture result_;

  // If nonzero, when a new finger arrived that may want us to stop scroll.
  stime_t finger_arrived_time_;

  // How long to wait when new fingers arrive (and possibly scroll), before
  // halting fling
  DoubleProperty fling_stop_timeout_;
};

}  // namespace gestures

#endif  // GESTURES_FLING_STOP_FILTER_INTERPRETER_H_

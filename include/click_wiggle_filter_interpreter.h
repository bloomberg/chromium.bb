// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/map.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_CLICK_WIGGLE_FILTER_INTERPRETER_H_
#define GESTURES_CLICK_WIGGLE_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter looks for accidental wiggle that occurs near a physical
// button click event and suppresses that motion.

class ClickWiggleFilterInterpreter : public Interpreter {
  FRIEND_TEST(ClickWiggleFilterInterpreterTest, SimpleTest);
  FRIEND_TEST(ClickWiggleFilterInterpreterTest, OneFingerClickSuppressTest);

 public:
  struct ClickWiggleRec {
    float x_, y_;  // position where we started blocking wiggle
    stime_t began_press_suppression_;  // when we started blocking inc/dec press
    bool suppress_inc_press_:1;
    bool suppress_dec_press_:1;
    bool operator==(const ClickWiggleRec& that) const {
      return x_ == that.x_ &&
          y_ == that.y_ &&
          began_press_suppression_ == that.began_press_suppression_ &&
          suppress_inc_press_ == that.suppress_inc_press_ &&
          suppress_dec_press_ == that.suppress_dec_press_;
    }
    bool operator!=(const ClickWiggleRec& that) const {
      return !(*this == that);
    }
  };

  // Takes ownership of |next|:
  ClickWiggleFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~ClickWiggleFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  void UpdateClickWiggle(const HardwareState& hwstate);
  void SetWarpFlags(HardwareState* hwstate) const;

  scoped_ptr<Interpreter> next_;

  map<short, ClickWiggleRec, kMaxFingers> wiggle_recs_;

  // last time a physical button up or down edge occurred
  stime_t button_edge_occurred_;

  map<short, float, kMaxFingers> prev_pressure_;

  int prev_buttons_;

  // Movements that may be tap/click wiggle larger than this are allowed
  DoubleProperty wiggle_max_dist_;
  // Wiggles (while button up) suppressed only for this time length.
  DoubleProperty wiggle_suppress_timeout_;
  // Wiggles after physical button going down are suppressed for this time
  DoubleProperty wiggle_button_down_timeout_;
  // Time [s] to block single-finger movement after a single finger clicks
  // the physical button down.
  DoubleProperty one_finger_click_wiggle_timeout_;
};

}  // namespace gestures

#endif  // GESTURES_CLICK_WIGGLE_FILTER_INTERPRETER_H_

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_SEMI_MT_CORRECTING_FILTER_INTERPRETER_H_
#define GESTURES_SEMI_MT_CORRECTING_FILTER_INTERPRETER_H_

namespace gestures {

static const size_t kMaxSemiMtFingers = 2;

// This interpreter processes incoming input events which required some
// tweaking due to the limitation of profile sensor, i.e. the semi-mt touchpad
// on Cr48 before they are processed by other interpreters. The tweaks mainly
// include low-pressure filtering, hysteresis, finger position correction.

class SemiMtCorrectingFilterInterpreter : public Interpreter {
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, LowPressureTest);
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, TrackingIdMappingTest);

 public:
  SemiMtCorrectingFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~SemiMtCorrectingFilterInterpreter() {}
  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);
  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);
  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  // As the Synaptics touchpad on Cr48 is very sensitive, we want to avoid the
  // hovering finger to be gone and back with the same tracking id. In addition
  // to reassign a new tracking id for the case, the function also supports
  // hysteresis and removes finger(s) with low pressure from the HardwareState.
  void LowPressureFilter(HardwareState* hwstate);

  // Update finger tracking ids.
  void AssignTrackingId(HardwareState* hwstate);

  // Previous HardwareState.
  HardwareState prev_hwstate_;

  // Previous Fingers.
  FingerState prev_fingers_[kMaxSemiMtFingers];

  // The last used id number.
  unsigned short last_id_;

  bool is_semi_mt_device_;
  BoolProperty interpreter_enabled_;
  DoubleProperty pressure_threshold_;
  DoubleProperty hysteresis_pressure_;
  scoped_ptr<Interpreter> next_;
};

}  // namespace gestures

#endif  // SEMI_MT_PREPROCESS_INTERPRETER_H_

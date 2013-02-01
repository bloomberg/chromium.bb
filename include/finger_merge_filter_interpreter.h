// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/set.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_FINGER_MERGE_FILTER_INTERPRETER_H_
#define GESTURES_FINGER_MERGE_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter mainly detects finger merging and mark the merging
// finger(s) with a new flag GESTURE_FINGER_MERGE. The flag can be used
// in immediateinterpreter to suppress cursor jumps/moves caused by the
// merging/merged finger(s).

class FingerMergeFilterInterpreter : public FilterInterpreter {

 public:
  FingerMergeFilterInterpreter(PropRegistry* prop_reg, Interpreter* next,
                               Tracer* tracer);
  virtual ~FingerMergeFilterInterpreter() {}

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate,
                                     stime_t* timeout);
  virtual void SetHardwarePropertiesImpl(const HardwareProperties& hwprops);

 private:
  // Detects finger merge and appends GESTURE_FINGER_MERGE flag for a merged
  // finger or close fingers
  void UpdateFingerMergeState(const HardwareState& hwstate);

  set<short, kMaxFingers> merge_tracking_ids_;

  // Flag to turn on/off the finger merge filter
  BoolProperty finger_merge_filter_enable_;

  // Distance threshold for close fingers
  DoubleProperty merge_distance_threshold_;

  // Maximum pressure value of a merged finger candidate
  DoubleProperty max_pressure_threshold_;

  // Minimum touch major of a merged finger candidate
  DoubleProperty min_major_threshold_;

  // More criteria for filtering out false positives:
  // The touch major of a merged finger could be merged_major_pressure_ratio_
  // times of its pressure value at least or a merged finger could have a
  // very high touch major
  DoubleProperty merged_major_pressure_ratio_;
  DoubleProperty merged_major_threshold_;
};

}

#endif

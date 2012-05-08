// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_FINGER_METRICS_H_
#define GESTURES_FINGER_METRICS_H_

#include "gestures/include/gestures.h"
#include "gestures/include/prop_registry.h"

namespace gestures {

// This class can contain methods that are used by multiple interpreters to
// evaluate fingers.

static const size_t kMaxFingers = 5;
static const size_t kMaxGesturingFingers = 3;
static const size_t kMaxTapFingers = 5;

class FingerMetrics {
 public:
  FingerMetrics(PropRegistry* prop_reg);

  // Returns true if the two fingers are near each other enough to do a multi
  // finger gesture together.
  bool FingersCloseEnoughToGesture(const FingerState& finger_a,
                                   const FingerState& finger_b) const;

 private:
  // Maximum distance [mm] two fingers may be separated and still be eligible
  // for a two-finger gesture (e.g., scroll / tap / click). These define an
  // ellipse with horizontal and vertical axes lengths (think: radii).
  DoubleProperty two_finger_close_horizontal_distance_thresh_;
  DoubleProperty two_finger_close_vertical_distance_thresh_;

  DISALLOW_COPY_AND_ASSIGN(FingerMetrics);
};

}  // namespace gestures

#endif  // GESTURES_FINGER_METRICS_H_

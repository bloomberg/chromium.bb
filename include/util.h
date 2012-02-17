// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_UTIL_H_
#define GESTURES_UTIL_H_

#include <math.h>

#include "gestures/include/gestures.h"

namespace gestures {

inline bool FloatEq(float a, float b) {
  return fabsf(a - b) <= 1e-5;
}

inline bool DoubleEq(float a, float b) {
  return fabsf(a - b) <= 1e-8;
}

// Returns the square of the distance between the two contacts.
template<typename ContactType>  // UnmergedContact or FingerState
static float DistSq(const ContactType& finger_a,
                    const FingerState& finger_b) {
  float dx = finger_a.position_x - finger_b.position_x;
  float dy = finger_a.position_y - finger_b.position_y;
  return dx * dx + dy * dy;
}
template<typename ContactType>  // UnmergedContact or FingerState
static float DistSqXY(const ContactType& finger_a, float pos_x, float pos_y) {
  float dx = finger_a.position_x - pos_x;
  float dy = finger_a.position_y - pos_y;
  return dx * dx + dy * dy;
}

}  // namespace gestures

#endif  // GESTURES_UTIL_H_

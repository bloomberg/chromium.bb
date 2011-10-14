// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_UTIL_H_
#define GESTURES_UTIL_H_

#include <math.h>

namespace gestures {

inline bool FloatEq(float a, float b) {
  return fabsf(a - b) <= 1e-5;
}

inline bool DoubleEq(float a, float b) {
  return fabsf(a - b) <= 1e-8;
}

}  // namespace gestures

#endif  // GESTURES_UTIL_H_

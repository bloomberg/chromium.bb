// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "content/public/common/page_zoom.h"

namespace content {

const double kMinimumZoomFactor = 0.25;
const double kMaximumZoomFactor = 5.0;

bool ZoomValuesEqual(double value_a, double value_b) {
  // Epsilon value for comparing two floating-point zoom values. We don't use
  // std::numeric_limits<> because it is too precise for zoom values. Zoom
  // values lose precision due to factor/level conversions. A value of 0.001
  // is precise enough for zoom value comparisons.
  const double epsilon = 0.001;

  return (std::fabs(value_a - value_b) <= epsilon);
}

}  // namespace content


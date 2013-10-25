// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_gesture_params.h"

namespace content {

SyntheticGestureParams::SyntheticGestureParams()
    : gesture_source_type(DEFAULT_INPUT) {}

SyntheticGestureParams::SyntheticGestureParams(
    const SyntheticGestureParams& other)
    : gesture_source_type(other.gesture_source_type) {}

SyntheticGestureParams::~SyntheticGestureParams() {}

}  // namespace content

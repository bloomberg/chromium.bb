// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

struct CONTENT_EXPORT SyntheticSmoothScrollGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticSmoothScrollGestureParams();
  SyntheticSmoothScrollGestureParams(
      const SyntheticSmoothScrollGestureParams& other);
  virtual ~SyntheticSmoothScrollGestureParams();

  virtual GestureType GetGestureType() const OVERRIDE;

  int distance;
  int anchor_x;
  int anchor_y;

  static const SyntheticSmoothScrollGestureParams* Cast(
      const SyntheticGestureParams* gesture_params);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_

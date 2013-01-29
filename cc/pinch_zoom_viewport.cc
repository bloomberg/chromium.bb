// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/pinch_zoom_viewport.h"

#include "base/logging.h"

namespace cc {

PinchZoomViewport::PinchZoomViewport()
    : page_scale_factor_(1),
      page_scale_delta_(1),
      sent_page_scale_delta_(1),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      device_scale_factor_(1) {
}

void PinchZoomViewport::set_page_scale_delta(float delta) {
  // Clamp to the current min/max limits.
  float totalPageScaleFactor = page_scale_factor_ * delta;
  if (min_page_scale_factor_ && totalPageScaleFactor < min_page_scale_factor_)
    delta = min_page_scale_factor_ / page_scale_factor_;
  else if (max_page_scale_factor_ &&
           totalPageScaleFactor > max_page_scale_factor_)
    delta = max_page_scale_factor_ / page_scale_factor_;

  if (delta == page_scale_delta_)
    return;

  page_scale_delta_ = delta;
}

bool PinchZoomViewport::SetPageScaleFactorAndLimits(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  DCHECK(page_scale_factor);

  if (sent_page_scale_delta_ == 1 && page_scale_factor == page_scale_factor_ &&
      min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return false;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;

  page_scale_factor_ = page_scale_factor;
  return true;
}

gfx::Transform PinchZoomViewport::ImplTransform(
    bool page_scale_pinch_zoom_enabled) const {
  gfx::Transform transform;
  transform.Scale(page_scale_delta_, page_scale_delta_);

  if (page_scale_pinch_zoom_enabled)
    transform.Scale(page_scale_factor_, page_scale_factor_);

  return transform;
}

}  // namespace cc

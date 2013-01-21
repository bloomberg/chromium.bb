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

gfx::RectF PinchZoomViewport::ZoomedViewport() const {
  gfx::SizeF layout_space_device_viewport_size = gfx::ScaleSize(
      device_viewport_size_,
      1 / (device_scale_factor_ * total_page_scale_factor()));
  return gfx::RectF(gfx::PointAtOffsetFromOrigin(zoomed_viewport_offset_),
                    layout_space_device_viewport_size);
}

gfx::Vector2dF PinchZoomViewport::ApplyScroll(const gfx::Vector2dF delta) {
  gfx::Vector2dF overflow;
  gfx::RectF pinched_bounds = ZoomedViewport() + delta;

  if (pinched_bounds.x() < 0) {
    overflow.set_x(pinched_bounds.x());
    pinched_bounds.set_x(0);
  }

  if (pinched_bounds.y() < 0) {
    overflow.set_y(pinched_bounds.y());
    pinched_bounds.set_y(0);
  }

  if (pinched_bounds.right() > layout_viewport_size_.width()) {
    overflow.set_x(pinched_bounds.right() - layout_viewport_size_.width());
    pinched_bounds += gfx::Vector2dF(
      layout_viewport_size_.width() - pinched_bounds.right(), 0);
  }

  if (pinched_bounds.bottom() > layout_viewport_size_.height()) {
    overflow.set_y(pinched_bounds.bottom() - layout_viewport_size_.height());
    pinched_bounds += gfx::Vector2dF(
      0, layout_viewport_size_.height() - pinched_bounds.bottom());
  }
  zoomed_viewport_offset_ = pinched_bounds.OffsetFromOrigin();

  return overflow;
}

gfx::Transform PinchZoomViewport::ImplTransform(
    bool page_scale_pinch_zoom_enabled) const {
  gfx::Transform transform;
  transform.Scale(page_scale_delta_, page_scale_delta_);

  // If the pinch state is applied in the impl, then push it to the
  // impl transform, otherwise the scale is handled by WebCore.
  if (page_scale_pinch_zoom_enabled) {
    transform.Scale(page_scale_factor_, page_scale_factor_);
    // The offset needs to be scaled by deviceScaleFactor as this transform
    // needs to work with physical pixels.
    gfx::Vector2dF zoomed_device_viewport_offset
      = gfx::ScaleVector2d(zoomed_viewport_offset_, device_scale_factor_);
    transform.Translate(-zoomed_device_viewport_offset.x(),
                        -zoomed_device_viewport_offset.y());
  }

  return transform;
}

}  // namespace cc

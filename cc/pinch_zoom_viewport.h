// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PINCH_ZOOM_VIEWPORT_H_
#define CC_PINCH_ZOOM_VIEWPORT_H_

#include "cc/cc_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

class CC_EXPORT PinchZoomViewport {
 public:
  PinchZoomViewport();

  float total_page_scale_factor() const {
    return page_scale_factor_ * page_scale_delta_;
  }

  void set_page_scale_factor(float factor) { page_scale_factor_ = factor; }
  float page_scale_factor() const { return page_scale_factor_; }

  void set_page_scale_delta(float delta);
  float page_scale_delta() const  { return page_scale_delta_; }

  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  void set_sent_page_scale_delta(float delta) {
    sent_page_scale_delta_ = delta;
  }

  float sent_page_scale_delta() const { return sent_page_scale_delta_; }

  void set_device_scale_factor(float factor) { device_scale_factor_ = factor; }
  float device_scale_factor() const { return device_scale_factor_; }

  // Returns true if the passed parameters were different from those previously
  // cached.
  bool SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);

  void set_layout_viewport_size(const gfx::SizeF& size) {
    layout_viewport_size_ = size;
  }
  // We need to store device_viewport_size separately because in mobile
  // fixed-layout mode, there is not necessarily a simple mapping between layout
  // viewport size and device viewport size.
  void set_device_viewport_size(const gfx::SizeF& size) {
    device_viewport_size_ = size;
  }

  // The implTransform applies the page scale transformation.
  //
  // implTransform = S[pageScaleFactor] * S[pageScaleDelta]
  gfx::Transform ImplTransform(bool page_scale_pinch_zoom_enabled) const;

 private:
  float page_scale_factor_;
  float page_scale_delta_;
  float sent_page_scale_delta_;
  float max_page_scale_factor_;
  float min_page_scale_factor_;
  float device_scale_factor_;

  gfx::SizeF layout_viewport_size_;
  gfx::SizeF device_viewport_size_;
};

}  // namespace cc

#endif  // CC_PINCH_ZOOM_VIEWPORT_H_

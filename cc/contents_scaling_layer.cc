// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/contents_scaling_layer.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

gfx::Size ContentsScalingLayer::computeContentBoundsForScale(float scaleX, float scaleY) const {
  return gfx::ToCeiledSize(gfx::ScaleSize(bounds(), scaleX, scaleY));
}

ContentsScalingLayer::ContentsScalingLayer() {
}

ContentsScalingLayer::~ContentsScalingLayer() {
}

void ContentsScalingLayer::calculateContentsScale(
    float ideal_contents_scale,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  *contents_scale_x = ideal_contents_scale;
  *contents_scale_y = ideal_contents_scale;
  *content_bounds = computeContentBoundsForScale(
      ideal_contents_scale,
      ideal_contents_scale);
}

void ContentsScalingLayer::didUpdateBounds() {
  drawProperties().content_bounds = computeContentBoundsForScale(
      contentsScaleX(),
      contentsScaleY());
}

}  // namespace cc

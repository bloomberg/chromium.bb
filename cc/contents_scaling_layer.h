// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CONTENTS_SCALING_LAYER_H_
#define CC_CONTENTS_SCALING_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

// Base class for layers that need contents scale.
// The content bounds are determined by bounds and scale of the contents.
class CC_EXPORT ContentsScalingLayer : public Layer {
 public:
  virtual void calculateContentsScale(
      float ideal_contents_scale,
      bool animating_transform_to_screen,
      float* contents_scale_x,
      float* contents_scale_y,
      gfx::Size* content_bounds) OVERRIDE;

  virtual void update(
    ResourceUpdateQueue& queue,
    const OcclusionTracker* occlusion,
    RenderingStats* stats) OVERRIDE;

 protected:
  ContentsScalingLayer();
  virtual ~ContentsScalingLayer();

  gfx::Size computeContentBoundsForScale(float scaleX, float scaleY) const;

 private:
  float last_update_contents_scale_x_;
  float last_update_contents_scale_y_;
};

}  // namespace cc

#endif  // CC_CONTENTS_SCALING_LAYER_H__

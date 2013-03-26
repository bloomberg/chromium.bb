// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_
#define CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_

#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

// A subclass to expose the total current occlusion.
template <typename LayerType, typename RenderSurfaceType>
class TestOcclusionTrackerBase
    : public OcclusionTrackerBase<LayerType, RenderSurfaceType> {
 public:
  TestOcclusionTrackerBase(gfx::Rect screen_scissor_rect,
                           bool record_metrics_for_frame)
      : OcclusionTrackerBase<LayerType, RenderSurfaceType>(
            screen_scissor_rect,
            record_metrics_for_frame) {}

  Region occlusion_from_inside_target() const {
    return OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().
        occlusion_from_inside_target;
  }
  Region occlusion_from_outside_target() const {
    return OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().
        occlusion_from_outside_target;
  }

  void set_occlusion_from_outside_target(const Region& region) {
    OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().
        occlusion_from_outside_target = region;
  }
  void set_occlusion_from_inside_target(const Region& region) {
    OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().
        occlusion_from_inside_target = region;
  }
};

typedef TestOcclusionTrackerBase<Layer, RenderSurface> TestOcclusionTracker;
typedef TestOcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>
    TestOcclusionTrackerImpl;

}  // namespace cc

#endif  // CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_OCCLUSION_TRACKER_H_
#define CC_TEST_TEST_OCCLUSION_TRACKER_H_

#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

// A subclass to expose the total current occlusion.
template <typename LayerType>
class TestOcclusionTracker : public OcclusionTracker<LayerType> {
 public:
  TestOcclusionTracker(const gfx::Rect& screen_scissor_rect,
                       bool record_metrics_for_frame)
      : OcclusionTracker<LayerType>(screen_scissor_rect,
                                    record_metrics_for_frame) {}

  Region occlusion_from_inside_target() const {
    return OcclusionTracker<LayerType>::stack_.back()
        .occlusion_from_inside_target;
  }
  Region occlusion_from_outside_target() const {
    return OcclusionTracker<LayerType>::stack_.back()
        .occlusion_from_outside_target;
  }

  void set_occlusion_from_outside_target(const Region& region) {
    OcclusionTracker<LayerType>::stack_.back().occlusion_from_outside_target =
        region;
  }
  void set_occlusion_from_inside_target(const Region& region) {
    OcclusionTracker<LayerType>::stack_.back().occlusion_from_inside_target =
        region;
  }
};

}  // namespace cc

#endif  // CC_TEST_TEST_OCCLUSION_TRACKER_H_

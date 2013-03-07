// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_
#define CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_

#include "cc/occlusion_tracker.h"
#include "cc/render_surface.h"
#include "cc/render_surface_impl.h"

namespace cc {

// A subclass to expose the total current occlusion.
template<typename LayerType, typename RenderSurfaceType>
class TestOcclusionTrackerBase : public cc::OcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestOcclusionTrackerBase(gfx::Rect screenScissorRect, bool recordMetricsForFrame = false)
        : cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>(screenScissorRect, recordMetricsForFrame)
    {
    }

    cc::Region occlusion_from_inside_target() const { return cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().occlusion_from_inside_target; }
    cc::Region occlusion_from_outside_target() const { return cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().occlusion_from_outside_target; }

    void setOcclusionFromOutsideTarget(const cc::Region& region) { cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().occlusion_from_outside_target = region; }
    void setOcclusionFromInsideTarget(const cc::Region& region) { cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::stack_.back().occlusion_from_inside_target = region; }
};

typedef TestOcclusionTrackerBase<cc::Layer, cc::RenderSurface> TestOcclusionTracker;
typedef TestOcclusionTrackerBase<cc::LayerImpl, cc::RenderSurfaceImpl> TestOcclusionTrackerImpl;

}  // namespace cc

#endif  // CC_TEST_OCCLUSION_TRACKER_TEST_COMMON_H_

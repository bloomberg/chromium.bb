// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCOcclusionTrackerTestCommon_h
#define CCOcclusionTrackerTestCommon_h

#include "CCRenderSurface.h"
#include "IntRect.h"
#include "Region.h"
#include "cc/occlusion_tracker.h"
#include "cc/render_surface.h"

namespace WebKitTests {

// A subclass to expose the total current occlusion.
template<typename LayerType, typename RenderSurfaceType>
class TestOcclusionTrackerBase : public cc::OcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestOcclusionTrackerBase(cc::IntRect screenScissorRect, bool recordMetricsForFrame = false)
        : cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>(screenScissorRect, recordMetricsForFrame)
    {
    }

    cc::Region occlusionInScreenSpace() const { return cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen; }
    cc::Region occlusionInTargetSurface() const { return cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const cc::Region& region) { cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const cc::Region& region) { cc::OcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget = region; }
};

typedef TestOcclusionTrackerBase<cc::Layer, cc::RenderSurface> TestOcclusionTracker;
typedef TestOcclusionTrackerBase<cc::LayerImpl, cc::RenderSurfaceImpl> TestOcclusionTrackerImpl;

}

#endif // CCOcclusionTrackerTestCommon_h

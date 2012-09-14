// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCOcclusionTrackerTestCommon_h
#define CCOcclusionTrackerTestCommon_h

#include "CCOcclusionTracker.h"
#include "CCRenderSurface.h"
#include "IntRect.h"
#include "Region.h"
#include "RenderSurfaceChromium.h"

namespace WebKitTests {

// A subclass to expose the total current occlusion.
template<typename LayerType, typename RenderSurfaceType>
class TestCCOcclusionTrackerBase : public cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestCCOcclusionTrackerBase(cc::IntRect screenScissorRect, bool recordMetricsForFrame = false)
        : cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>(screenScissorRect, recordMetricsForFrame)
    {
    }

    cc::Region occlusionInScreenSpace() const { return cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen; }
    cc::Region occlusionInTargetSurface() const { return cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const cc::Region& region) { cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const cc::Region& region) { cc::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget = region; }
};

typedef TestCCOcclusionTrackerBase<cc::LayerChromium, cc::RenderSurfaceChromium> TestCCOcclusionTracker;
typedef TestCCOcclusionTrackerBase<cc::CCLayerImpl, cc::CCRenderSurface> TestCCOcclusionTrackerImpl;

}

#endif // CCOcclusionTrackerTestCommon_h

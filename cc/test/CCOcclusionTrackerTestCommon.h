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
class TestCCOcclusionTrackerBase : public WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestCCOcclusionTrackerBase(WebCore::IntRect screenScissorRect, bool recordMetricsForFrame = false)
        : WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>(screenScissorRect, recordMetricsForFrame)
    {
    }

    WebCore::Region occlusionInScreenSpace() const { return WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen; }
    WebCore::Region occlusionInTargetSurface() const { return WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const WebCore::Region& region) { WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const WebCore::Region& region) { WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget = region; }
};

typedef TestCCOcclusionTrackerBase<WebCore::LayerChromium, WebCore::RenderSurfaceChromium> TestCCOcclusionTracker;
typedef TestCCOcclusionTrackerBase<WebCore::CCLayerImpl, WebCore::CCRenderSurface> TestCCOcclusionTrackerImpl;

}

#endif // CCOcclusionTrackerTestCommon_h

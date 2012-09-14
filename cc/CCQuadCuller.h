// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCQuadCuller_h
#define CCQuadCuller_h

#include "CCQuadSink.h"
#include "CCRenderPass.h"

namespace cc {
class CCLayerImpl;
class CCRenderSurface;
template<typename LayerType, typename SurfaceType>
class CCOcclusionTrackerBase;

class CCQuadCuller : public CCQuadSink {
public:
    CCQuadCuller(CCQuadList&, CCSharedQuadStateList&, CCLayerImpl*, const CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>*, bool showCullingWithDebugBorderQuads, bool forSurface);
    virtual ~CCQuadCuller() { }

    // CCQuadSink implementation.
    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState>) OVERRIDE;
    virtual bool append(PassOwnPtr<CCDrawQuad>, CCAppendQuadsData&) OVERRIDE;

private:
    CCQuadList& m_quadList;
    CCSharedQuadStateList& m_sharedQuadStateList;
    CCSharedQuadState* m_currentSharedQuadState;
    CCLayerImpl* m_layer;
    const CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>* m_occlusionTracker;
    bool m_showCullingWithDebugBorderQuads;
    bool m_forSurface;
};

}
#endif // CCQuadCuller_h

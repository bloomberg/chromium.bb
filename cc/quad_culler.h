// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUAD_CULLER_H_
#define CC_QUAD_CULLER_H_

#include "cc/cc_export.h"
#include "cc/quad_sink.h"
#include "cc/render_pass.h"

namespace cc {
class LayerImpl;
class RenderSurfaceImpl;
template<typename LayerType, typename SurfaceType>
class OcclusionTrackerBase;

class CC_EXPORT QuadCuller : public QuadSink {
public:
    QuadCuller(QuadList&, SharedQuadStateList&, const LayerImpl*, const OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>&, bool showCullingWithDebugBorderQuads, bool forSurface);
    virtual ~QuadCuller() { }

    // QuadSink implementation.
    virtual SharedQuadState* useSharedQuadState(scoped_ptr<SharedQuadState>) OVERRIDE;
    virtual bool append(scoped_ptr<DrawQuad>, AppendQuadsData*) OVERRIDE;

private:
    QuadList& m_quadList;
    SharedQuadStateList& m_sharedQuadStateList;
    const LayerImpl* m_layer;
    const OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>& m_occlusionTracker;

    SharedQuadState* m_currentSharedQuadState;
    bool m_showCullingWithDebugBorderQuads;
    bool m_forSurface;
};

}
#endif  // CC_QUAD_CULLER_H_

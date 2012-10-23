// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/quad_culler.h"

#include "CCAppendQuadsData.h"
#include "CCDebugBorderDrawQuad.h"
#include "Region.h"
#include "cc/layer_impl.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/render_pass.h"
#include "third_party/skia/include/core/SkColor.h"
#include <public/WebTransformationMatrix.h>

using namespace std;

namespace cc {

static const int debugTileBorderWidth = 1;
static const int debugTileBorderAlpha = 120;
static const int debugTileBorderColorRed = 160;
static const int debugTileBorderColorGreen = 100;
static const int debugTileBorderColorBlue = 0;

QuadCuller::QuadCuller(QuadList& quadList, SharedQuadStateList& sharedQuadStateList, LayerImpl* layer, const OcclusionTrackerImpl* occlusionTracker, bool showCullingWithDebugBorderQuads, bool forSurface)
    : m_quadList(quadList)
    , m_sharedQuadStateList(sharedQuadStateList)
    , m_currentSharedQuadState(0)
    , m_layer(layer)
    , m_occlusionTracker(occlusionTracker)
    , m_showCullingWithDebugBorderQuads(showCullingWithDebugBorderQuads)
    , m_forSurface(forSurface)
{
}

SharedQuadState* QuadCuller::useSharedQuadState(scoped_ptr<SharedQuadState> sharedQuadState)
{
    sharedQuadState->id = m_sharedQuadStateList.size();

    // FIXME: If all quads are culled for the sharedQuadState, we can drop it from the list.
    m_currentSharedQuadState = sharedQuadState.get();
    m_sharedQuadStateList.append(sharedQuadState.Pass());
    return m_currentSharedQuadState;
}

static inline bool appendQuadInternal(scoped_ptr<DrawQuad> drawQuad, const IntRect& culledRect, QuadList& quadList, const OcclusionTrackerImpl& occlusionTracker, bool createDebugBorderQuads)
{
    bool keepQuad = !culledRect.isEmpty();
    if (keepQuad)
        drawQuad->setQuadVisibleRect(culledRect);

    occlusionTracker.overdrawMetrics().didCullForDrawing(drawQuad->quadTransform(), cc::IntRect(drawQuad->quadRect()), culledRect);
    occlusionTracker.overdrawMetrics().didDraw(drawQuad->quadTransform(), culledRect, cc::IntRect(drawQuad->opaqueRect()));

    if (keepQuad) {
        if (createDebugBorderQuads && !drawQuad->isDebugQuad() && drawQuad->quadVisibleRect() != drawQuad->quadRect()) {
            SkColor borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderColorRed, debugTileBorderColorGreen, debugTileBorderColorBlue);
            quadList.append(DebugBorderDrawQuad::create(drawQuad->sharedQuadState(), drawQuad->quadVisibleRect(), borderColor, debugTileBorderWidth).PassAs<DrawQuad>());
        }

        // Pass the quad after we're done using it.
        quadList.append(drawQuad.Pass());
    }
    return keepQuad;
}

bool QuadCuller::append(scoped_ptr<DrawQuad> drawQuad, AppendQuadsData& appendQuadsData)
{
    DCHECK(drawQuad->sharedQuadState() == m_currentSharedQuadState);
    DCHECK(drawQuad->sharedQuadStateId() == m_currentSharedQuadState->id);
    DCHECK(!m_sharedQuadStateList.isEmpty());
    DCHECK(m_sharedQuadStateList.last() == m_currentSharedQuadState);

    IntRect culledRect;
    bool hasOcclusionFromOutsideTargetSurface;

    if (m_forSurface)
        culledRect = m_occlusionTracker->unoccludedContributingSurfaceContentRect(m_layer, false, cc::IntRect(drawQuad->quadRect()), &hasOcclusionFromOutsideTargetSurface);
    else
        culledRect = m_occlusionTracker->unoccludedContentRect(m_layer, cc::IntRect(drawQuad->quadRect()), &hasOcclusionFromOutsideTargetSurface);

    appendQuadsData.hadOcclusionFromOutsideTargetSurface |= hasOcclusionFromOutsideTargetSurface;

    return appendQuadInternal(drawQuad.Pass(), culledRect, m_quadList, *m_occlusionTracker, m_showCullingWithDebugBorderQuads);
}

} // namespace cc

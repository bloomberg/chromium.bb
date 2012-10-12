// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCQuadCuller.h"

#include "CCAppendQuadsData.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCLayerImpl.h"
#include "CCOcclusionTracker.h"
#include "CCOverdrawMetrics.h"
#include "CCRenderPass.h"
#include "Region.h"
#include "SkColor.h"
#include <public/WebTransformationMatrix.h>

using namespace std;

namespace cc {

static const int debugTileBorderWidth = 1;
static const int debugTileBorderAlpha = 120;
static const int debugTileBorderColorRed = 160;
static const int debugTileBorderColorGreen = 100;
static const int debugTileBorderColorBlue = 0;

CCQuadCuller::CCQuadCuller(CCQuadList& quadList, CCSharedQuadStateList& sharedQuadStateList, CCLayerImpl* layer, const CCOcclusionTrackerImpl* occlusionTracker, bool showCullingWithDebugBorderQuads, bool forSurface)
    : m_quadList(quadList)
    , m_sharedQuadStateList(sharedQuadStateList)
    , m_currentSharedQuadState(0)
    , m_layer(layer)
    , m_occlusionTracker(occlusionTracker)
    , m_showCullingWithDebugBorderQuads(showCullingWithDebugBorderQuads)
    , m_forSurface(forSurface)
{
}

CCSharedQuadState* CCQuadCuller::useSharedQuadState(scoped_ptr<CCSharedQuadState> sharedQuadState)
{
    sharedQuadState->id = m_sharedQuadStateList.size();

    // FIXME: If all quads are culled for the sharedQuadState, we can drop it from the list.
    m_currentSharedQuadState = sharedQuadState.get();
    m_sharedQuadStateList.append(sharedQuadState.Pass());
    return m_currentSharedQuadState;
}

static inline bool appendQuadInternal(scoped_ptr<CCDrawQuad> drawQuad, const IntRect& culledRect, CCQuadList& quadList, const CCOcclusionTrackerImpl& occlusionTracker, bool createDebugBorderQuads)
{
    bool keepQuad = !culledRect.isEmpty();
    if (keepQuad)
        drawQuad->setQuadVisibleRect(culledRect);

    occlusionTracker.overdrawMetrics().didCullForDrawing(drawQuad->quadTransform(), drawQuad->quadRect(), culledRect);
    occlusionTracker.overdrawMetrics().didDraw(drawQuad->quadTransform(), culledRect, drawQuad->opaqueRect());

    if (keepQuad) {
        if (createDebugBorderQuads && !drawQuad->isDebugQuad() && drawQuad->quadVisibleRect() != drawQuad->quadRect()) {
            SkColor borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderColorRed, debugTileBorderColorGreen, debugTileBorderColorBlue);
            quadList.append(CCDebugBorderDrawQuad::create(drawQuad->sharedQuadState(), drawQuad->quadVisibleRect(), borderColor, debugTileBorderWidth).PassAs<CCDrawQuad>());
        }

        // Pass the quad after we're done using it.
        quadList.append(drawQuad.Pass());
    }
    return keepQuad;
}

bool CCQuadCuller::append(scoped_ptr<CCDrawQuad> drawQuad, CCAppendQuadsData& appendQuadsData)
{
    ASSERT(drawQuad->sharedQuadState() == m_currentSharedQuadState);
    ASSERT(drawQuad->sharedQuadStateId() == m_currentSharedQuadState->id);
    ASSERT(!m_sharedQuadStateList.isEmpty());
    ASSERT(m_sharedQuadStateList.last() == m_currentSharedQuadState);

    IntRect culledRect;
    bool hasOcclusionFromOutsideTargetSurface;

    if (m_forSurface)
        culledRect = m_occlusionTracker->unoccludedContributingSurfaceContentRect(m_layer, false, drawQuad->quadRect(), &hasOcclusionFromOutsideTargetSurface);
    else
        culledRect = m_occlusionTracker->unoccludedContentRect(m_layer, drawQuad->quadRect(), &hasOcclusionFromOutsideTargetSurface);

    appendQuadsData.hadOcclusionFromOutsideTargetSurface |= hasOcclusionFromOutsideTargetSurface;

    return appendQuadInternal(drawQuad.Pass(), culledRect, m_quadList, *m_occlusionTracker, m_showCullingWithDebugBorderQuads);
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)

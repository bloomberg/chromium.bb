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

CCSharedQuadState* CCQuadCuller::useSharedQuadState(PassOwnPtr<CCSharedQuadState> passSharedQuadState)
{
    OwnPtr<CCSharedQuadState> sharedQuadState(passSharedQuadState);
    sharedQuadState->id = m_sharedQuadStateList.size();

    // FIXME: If all quads are culled for the sharedQuadState, we can drop it from the list.
    m_currentSharedQuadState = sharedQuadState.get();
    m_sharedQuadStateList.append(sharedQuadState.release());
    return m_currentSharedQuadState;
}

static inline bool appendQuadInternal(PassOwnPtr<CCDrawQuad> passDrawQuad, const IntRect& culledRect, CCQuadList& quadList, const CCOcclusionTrackerImpl& occlusionTracker, bool createDebugBorderQuads)
{
    OwnPtr<CCDrawQuad> drawQuad(passDrawQuad);
    bool keepQuad = !culledRect.isEmpty();
    if (keepQuad)
        drawQuad->setQuadVisibleRect(culledRect);

    occlusionTracker.overdrawMetrics().didCullForDrawing(drawQuad->quadTransform(), drawQuad->quadRect(), culledRect);
    occlusionTracker.overdrawMetrics().didDraw(drawQuad->quadTransform(), culledRect, drawQuad->opaqueRect());

    if (keepQuad) {
        if (createDebugBorderQuads && !drawQuad->isDebugQuad() && drawQuad->quadVisibleRect() != drawQuad->quadRect()) {
            SkColor borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderColorRed, debugTileBorderColorGreen, debugTileBorderColorBlue);
            quadList.append(CCDebugBorderDrawQuad::create(drawQuad->sharedQuadState(), drawQuad->quadVisibleRect(), borderColor, debugTileBorderWidth));
        }

        // Release the quad after we're done using it.
        quadList.append(drawQuad.release());
    }
    return keepQuad;
}

bool CCQuadCuller::append(PassOwnPtr<CCDrawQuad> passDrawQuad, CCAppendQuadsData& appendQuadsData)
{
    ASSERT(passDrawQuad->sharedQuadState() == m_currentSharedQuadState);
    ASSERT(passDrawQuad->sharedQuadStateId() == m_currentSharedQuadState->id);
    ASSERT(!m_sharedQuadStateList.isEmpty());
    ASSERT(m_sharedQuadStateList.last().get() == m_currentSharedQuadState);

    IntRect culledRect;
    bool hasOcclusionFromOutsideTargetSurface;

    if (m_forSurface)
        culledRect = m_occlusionTracker->unoccludedContributingSurfaceContentRect(m_layer, false, passDrawQuad->quadRect(), &hasOcclusionFromOutsideTargetSurface);
    else
        culledRect = m_occlusionTracker->unoccludedContentRect(m_layer, passDrawQuad->quadRect(), &hasOcclusionFromOutsideTargetSurface);

    appendQuadsData.hadOcclusionFromOutsideTargetSurface |= hasOcclusionFromOutsideTargetSurface;

    return appendQuadInternal(passDrawQuad, culledRect, m_quadList, *m_occlusionTracker, m_showCullingWithDebugBorderQuads);
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)

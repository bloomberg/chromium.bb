// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quad_culler.h"

#include "cc/append_quads_data.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/debug_colors.h"
#include "cc/layer_impl.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/render_pass.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/transform.h"

namespace cc {

QuadCuller::QuadCuller(QuadList& quadList, SharedQuadStateList& sharedQuadStateList, const LayerImpl* layer, const OcclusionTrackerImpl& occlusionTracker, bool showCullingWithDebugBorderQuads, bool forSurface)
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
    // FIXME: If all quads are culled for the sharedQuadState, we can drop it from the list.
    m_currentSharedQuadState = sharedQuadState.get();
    m_sharedQuadStateList.push_back(sharedQuadState.Pass());
    return m_currentSharedQuadState;
}

static inline bool appendQuadInternal(scoped_ptr<DrawQuad> drawQuad, const gfx::Rect& culledRect, QuadList& quadList, const OcclusionTrackerImpl& occlusionTracker, const LayerImpl* layer, bool createDebugBorderQuads)
{
    bool keepQuad = !culledRect.IsEmpty();
    if (keepQuad)
        drawQuad->visible_rect = culledRect;

    occlusionTracker.overdrawMetrics().didCullForDrawing(drawQuad->quadTransform(), drawQuad->rect, culledRect);
    gfx::Rect opaqueDrawRect = drawQuad->opacity() == 1.0f ? drawQuad->opaque_rect : gfx::Rect();
    occlusionTracker.overdrawMetrics().didDraw(drawQuad->quadTransform(), culledRect, opaqueDrawRect);

    if (keepQuad) {
        if (createDebugBorderQuads && !drawQuad->IsDebugQuad() && drawQuad->visible_rect != drawQuad->rect) {
            SkColor color = DebugColors::CulledTileBorderColor();
            float width = DebugColors::CulledTileBorderWidth(layer ? layer->layerTreeImpl() : NULL);
            scoped_ptr<DebugBorderDrawQuad> debugBorderQuad = DebugBorderDrawQuad::Create();
            debugBorderQuad->SetNew(drawQuad->shared_quad_state, drawQuad->visible_rect, color, width);
            quadList.push_back(debugBorderQuad.PassAs<DrawQuad>());
        }

        // Pass the quad after we're done using it.
        quadList.push_back(drawQuad.Pass());
    }
    return keepQuad;
}

bool QuadCuller::append(scoped_ptr<DrawQuad> drawQuad, AppendQuadsData& appendQuadsData)
{
    DCHECK(drawQuad->shared_quad_state == m_currentSharedQuadState);
    DCHECK(!m_sharedQuadStateList.empty());
    DCHECK(m_sharedQuadStateList.back() == m_currentSharedQuadState);

    gfx::Rect culledRect;
    bool hasOcclusionFromOutsideTargetSurface;
    bool implDrawTransformIsUnknown = false;

    if (m_forSurface)
        culledRect = m_occlusionTracker.unoccludedContributingSurfaceContentRect(m_layer, false, drawQuad->rect, &hasOcclusionFromOutsideTargetSurface);
    else
        culledRect = m_occlusionTracker.unoccludedContentRect(m_layer->renderTarget(), drawQuad->rect, drawQuad->quadTransform(), implDrawTransformIsUnknown, drawQuad->isClipped(), drawQuad->clipRect(), &hasOcclusionFromOutsideTargetSurface);

    appendQuadsData.hadOcclusionFromOutsideTargetSurface |= hasOcclusionFromOutsideTargetSurface;

    return appendQuadInternal(drawQuad.Pass(), culledRect, m_quadList, m_occlusionTracker, m_layer, m_showCullingWithDebugBorderQuads);
}

}  // namespace cc

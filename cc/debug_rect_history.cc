// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug_rect_history.h"

#include "cc/damage_tracker.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host.h"
#include "cc/math_util.h"

namespace cc {

// static
scoped_ptr<DebugRectHistory> DebugRectHistory::create() {
  return make_scoped_ptr(new DebugRectHistory());
}

DebugRectHistory::DebugRectHistory()
{
}

DebugRectHistory::~DebugRectHistory()
{
}

void DebugRectHistory::saveDebugRectsForCurrentFrame(LayerImpl* rootLayer, const std::vector<LayerImpl*>& renderSurfaceLayerList, const std::vector<gfx::Rect>& occludingScreenSpaceRects, const std::vector<gfx::Rect>& nonOccludingScreenSpaceRects, const LayerTreeDebugState& debugState)
{
    // For now, clear all rects from previous frames. In the future we may want to store
    // all debug rects for a history of many frames.
    m_debugRects.clear();

    if (debugState.showPaintRects)
        savePaintRects(rootLayer);

    if (debugState.showPropertyChangedRects)
        savePropertyChangedRects(renderSurfaceLayerList);

    if (debugState.showSurfaceDamageRects)
        saveSurfaceDamageRects(renderSurfaceLayerList);

    if (debugState.showScreenSpaceRects)
        saveScreenSpaceRects(renderSurfaceLayerList);

    if (debugState.showOccludingRects)
        saveOccludingRects(occludingScreenSpaceRects);

    if (debugState.showNonOccludingRects)
        saveNonOccludingRects(nonOccludingScreenSpaceRects);
}


void DebugRectHistory::savePaintRects(LayerImpl* layer)
{
    // We would like to visualize where any layer's paint rect (update rect) has changed,
    // regardless of whether this layer is skipped for actual drawing or not. Therefore
    // we traverse recursively over all layers, not just the render surface list.

    if (!layer->update_rect().IsEmpty() && layer->DrawsContent()) {
        float widthScale = layer->content_bounds().width() / static_cast<float>(layer->bounds().width());
        float heightScale = layer->content_bounds().height() / static_cast<float>(layer->bounds().height());
        gfx::RectF updateContentRect = gfx::ScaleRect(layer->update_rect(), widthScale, heightScale);
        m_debugRects.push_back(DebugRect(PaintRectType, MathUtil::mapClippedRect(layer->screen_space_transform(), updateContentRect)));
    }

    for (unsigned i = 0; i < layer->children().size(); ++i)
        savePaintRects(layer->children()[i]);
}

void DebugRectHistory::savePropertyChangedRects(const std::vector<LayerImpl*>& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        RenderSurfaceImpl* renderSurface = renderSurfaceLayer->render_surface();
        DCHECK(renderSurface);

        const std::vector<LayerImpl*>& layerList = renderSurface->layer_list();
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            LayerImpl* layer = layerList[layerIndex];

            if (LayerTreeHostCommon::renderSurfaceContributesToTarget<LayerImpl>(layer, renderSurfaceLayer->id()))
                continue;

            if (layer->LayerIsAlwaysDamaged())
                continue;

            if (layer->LayerPropertyChanged() || layer->LayerSurfacePropertyChanged())
                m_debugRects.push_back(DebugRect(PropertyChangedRectType, MathUtil::mapClippedRect(layer->screen_space_transform(), gfx::RectF(gfx::PointF(), layer->content_bounds()))));
        }
    }
}

void DebugRectHistory::saveSurfaceDamageRects(const std::vector<LayerImpl* >& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        RenderSurfaceImpl* renderSurface = renderSurfaceLayer->render_surface();
        DCHECK(renderSurface);

        m_debugRects.push_back(DebugRect(SurfaceDamageRectType, MathUtil::mapClippedRect(renderSurface->screen_space_transform(), renderSurface->damage_tracker()->current_damage_rect())));
    }
}

void DebugRectHistory::saveScreenSpaceRects(const std::vector<LayerImpl* >& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        RenderSurfaceImpl* renderSurface = renderSurfaceLayer->render_surface();
        DCHECK(renderSurface);

        m_debugRects.push_back(DebugRect(ScreenSpaceRectType, MathUtil::mapClippedRect(renderSurface->screen_space_transform(), renderSurface->content_rect())));

        if (renderSurfaceLayer->replica_layer())
            m_debugRects.push_back(DebugRect(ReplicaScreenSpaceRectType, MathUtil::mapClippedRect(renderSurface->replica_screen_space_transform(), renderSurface->content_rect())));
    }
}

void DebugRectHistory::saveOccludingRects(const std::vector<gfx::Rect>& occludingRects)
{
    for (size_t i = 0; i < occludingRects.size(); ++i)
        m_debugRects.push_back(DebugRect(OccludingRectType, occludingRects[i]));
}

void DebugRectHistory::saveNonOccludingRects(const std::vector<gfx::Rect>& nonOccludingRects)
{
    for (size_t i = 0; i < nonOccludingRects.size(); ++i)
        m_debugRects.push_back(DebugRect(NonOccludingRectType, nonOccludingRects[i]));
}

}  // namespace cc

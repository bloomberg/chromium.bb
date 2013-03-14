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
scoped_ptr<DebugRectHistory> DebugRectHistory::Create() {
  return make_scoped_ptr(new DebugRectHistory());
}

DebugRectHistory::DebugRectHistory() {}

DebugRectHistory::~DebugRectHistory() {}

void DebugRectHistory::SaveDebugRectsForCurrentFrame(
    LayerImpl* root_layer,
    const std::vector<LayerImpl*>& render_surface_layer_list,
    const std::vector<gfx::Rect>& occluding_screen_space_rects,
    const std::vector<gfx::Rect>& non_occluding_screen_space_rects,
    const LayerTreeDebugState& debug_state) {
  // For now, clear all rects from previous frames. In the future we may want to
  // store all debug rects for a history of many frames.
  debug_rects_.clear();

  if (debug_state.showPaintRects)
    SavePaintRects(root_layer);

  if (debug_state.showPropertyChangedRects)
    SavePropertyChangedRects(render_surface_layer_list);

  if (debug_state.showSurfaceDamageRects)
    SaveSurfaceDamageRects(render_surface_layer_list);

  if (debug_state.showScreenSpaceRects)
    SaveScreenSpaceRects(render_surface_layer_list);

  if (debug_state.showOccludingRects)
    SaveOccludingRects(occluding_screen_space_rects);

  if (debug_state.showNonOccludingRects)
    SaveNonOccludingRects(non_occluding_screen_space_rects);
}

void DebugRectHistory::SavePaintRects(LayerImpl* layer) {
  // We would like to visualize where any layer's paint rect (update rect) has
  // changed, regardless of whether this layer is skipped for actual drawing or
  // not. Therefore we traverse recursively over all layers, not just the render
  // surface list.

  if (!layer->update_rect().IsEmpty() && layer->DrawsContent()) {
    float width_scale = layer->content_bounds().width() /
                        static_cast<float>(layer->bounds().width());
    float height_scale = layer->content_bounds().height() /
                         static_cast<float>(layer->bounds().height());
    gfx::RectF update_content_rect =
        gfx::ScaleRect(layer->update_rect(), width_scale, height_scale);
    debug_rects_.push_back(
        DebugRect(PAINT_RECT_TYPE,
                  MathUtil::mapClippedRect(layer->screen_space_transform(),
                                           update_content_rect)));
  }

  for (unsigned i = 0; i < layer->children().size(); ++i)
    SavePaintRects(layer->children()[i]);
}

void DebugRectHistory::SavePropertyChangedRects(
    const std::vector<LayerImpl*>& render_surface_layer_list) {
  for (int surface_index = render_surface_layer_list.size() - 1;
       surface_index >= 0;
       --surface_index) {
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);

    const std::vector<LayerImpl*>& layer_list = render_surface->layer_list();
    for (unsigned layer_index = 0;
         layer_index < layer_list.size();
         ++layer_index) {
      LayerImpl* layer = layer_list[layer_index];

      if (LayerTreeHostCommon::renderSurfaceContributesToTarget<LayerImpl>(
              layer, render_surface_layer->id()))
        continue;

      if (layer->LayerIsAlwaysDamaged())
        continue;

      if (layer->LayerPropertyChanged() ||
          layer->LayerSurfacePropertyChanged()) {
        debug_rects_.push_back(
            DebugRect(PROPERTY_CHANGED_RECT_TYPE,
                      MathUtil::mapClippedRect(
                          layer->screen_space_transform(),
                          gfx::RectF(gfx::PointF(), layer->content_bounds()))));
      }
    }
  }
}

void DebugRectHistory::SaveSurfaceDamageRects(
    const std::vector<LayerImpl*>& render_surface_layer_list) {
  for (int surface_index = render_surface_layer_list.size() - 1;
       surface_index >= 0;
       --surface_index) {
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);

    debug_rects_.push_back(DebugRect(
        SURFACE_DAMAGE_RECT_TYPE,
        MathUtil::mapClippedRect(
            render_surface->screen_space_transform(),
            render_surface->damage_tracker()->current_damage_rect())));
  }
}

void DebugRectHistory::SaveScreenSpaceRects(
    const std::vector<LayerImpl*>& render_surface_layer_list) {
  for (int surface_index = render_surface_layer_list.size() - 1;
       surface_index >= 0;
       --surface_index) {
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);

    debug_rects_.push_back(DebugRect(
        SCREEN_SPACE_RECT_TYPE,
        MathUtil::mapClippedRect(render_surface->screen_space_transform(),
                                 render_surface->content_rect())));

    if (render_surface_layer->replica_layer()) {
      debug_rects_.push_back(
          DebugRect(REPLICA_SCREEN_SPACE_RECT_TYPE,
                    MathUtil::mapClippedRect(
                        render_surface->replica_screen_space_transform(),
                        render_surface->content_rect())));
    }
  }
}

void DebugRectHistory::SaveOccludingRects(
    const std::vector<gfx::Rect>& occluding_rects) {
  for (size_t i = 0; i < occluding_rects.size(); ++i)
    debug_rects_.push_back(DebugRect(OCCLUDING_RECT_TYPE, occluding_rects[i]));
}

void DebugRectHistory::SaveNonOccludingRects(
    const std::vector<gfx::Rect>& non_occluding_rects) {
  for (size_t i = 0; i < non_occluding_rects.size(); ++i) {
    debug_rects_.push_back(
        DebugRect(NONOCCLUDING_RECT_TYPE, non_occluding_rects[i]));
  }
}

}  // namespace cc

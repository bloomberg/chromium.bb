// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/debug_rect_history.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_host.h"

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

  if (debug_state.show_paint_rects)
    SavePaintRects(root_layer);

  if (debug_state.show_property_changed_rects)
    SavePropertyChangedRects(render_surface_layer_list);

  if (debug_state.show_surface_damage_rects)
    SaveSurfaceDamageRects(render_surface_layer_list);

  if (debug_state.show_screen_space_rects)
    SaveScreenSpaceRects(render_surface_layer_list);

  if (debug_state.show_occluding_rects)
    SaveOccludingRects(occluding_screen_space_rects);

  if (debug_state.show_non_occluding_rects)
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
                  MathUtil::MapClippedRect(layer->screen_space_transform(),
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

      if (LayerTreeHostCommon::RenderSurfaceContributesToTarget<LayerImpl>(
              layer, render_surface_layer->id()))
        continue;

      if (layer->LayerIsAlwaysDamaged())
        continue;

      if (layer->LayerPropertyChanged() ||
          layer->LayerSurfacePropertyChanged()) {
        debug_rects_.push_back(
            DebugRect(PROPERTY_CHANGED_RECT_TYPE,
                      MathUtil::MapClippedRect(
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
        MathUtil::MapClippedRect(
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
        MathUtil::MapClippedRect(render_surface->screen_space_transform(),
                                 render_surface->content_rect())));

    if (render_surface_layer->replica_layer()) {
      debug_rects_.push_back(
          DebugRect(REPLICA_SCREEN_SPACE_RECT_TYPE,
                    MathUtil::MapClippedRect(
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

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/quad_culler.h"

#include "cc/debug/debug_colors.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/trees/occlusion_tracker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/transform.h"

namespace cc {

QuadCuller::QuadCuller(QuadList* quad_list,
                       SharedQuadStateList* shared_quad_state_list,
                       const LayerImpl* layer,
                       const OcclusionTrackerImpl& occlusion_tracker,
                       bool show_culling_with_debug_border_quads,
                       bool for_surface)
    : quad_list_(quad_list),
      shared_quad_state_list_(shared_quad_state_list),
      current_shared_quad_state_(NULL),
      layer_(layer),
      occlusion_tracker_(occlusion_tracker),
      show_culling_with_debug_border_quads_(
          show_culling_with_debug_border_quads),
      for_surface_(for_surface) {}

SharedQuadState* QuadCuller::UseSharedQuadState(
    scoped_ptr<SharedQuadState> shared_quad_state) {
  // FIXME: If all quads are culled for the shared_quad_state, we can drop it
  // from the list.
  current_shared_quad_state_ = shared_quad_state.get();
  shared_quad_state_list_->push_back(shared_quad_state.Pass());
  return current_shared_quad_state_;
}

static inline bool AppendQuadInternal(
    scoped_ptr<DrawQuad> draw_quad,
    gfx::Rect culled_rect,
    QuadList* quad_list,
    const OcclusionTrackerImpl& occlusion_tracker,
    const LayerImpl* layer,
    bool create_debug_border_quads) {
  bool keep_quad = !culled_rect.IsEmpty();
  if (keep_quad)
    draw_quad->visible_rect = culled_rect;

  occlusion_tracker.overdraw_metrics()->DidCullForDrawing(
      draw_quad->quadTransform(), draw_quad->rect, culled_rect);
  gfx::Rect opaque_draw_rect =
      draw_quad->opacity() == 1.0f ? draw_quad->opaque_rect : gfx::Rect();
  occlusion_tracker.overdraw_metrics()->
      DidDraw(draw_quad->quadTransform(), culled_rect, opaque_draw_rect);

  if (keep_quad) {
    if (create_debug_border_quads && !draw_quad->IsDebugQuad() &&
        draw_quad->visible_rect != draw_quad->rect) {
      SkColor color = DebugColors::CulledTileBorderColor();
      float width = DebugColors::CulledTileBorderWidth(
          layer ? layer->layer_tree_impl() : NULL);
      scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
          DebugBorderDrawQuad::Create();
      debug_border_quad->SetNew(
          draw_quad->shared_quad_state, draw_quad->visible_rect, color, width);
      quad_list->push_back(debug_border_quad.PassAs<DrawQuad>());
    }

    // Pass the quad after we're done using it.
    quad_list->push_back(draw_quad.Pass());
  }
  return keep_quad;
}

bool QuadCuller::Append(scoped_ptr<DrawQuad> draw_quad,
                        AppendQuadsData* append_quads_data) {
  DCHECK(draw_quad->shared_quad_state == current_shared_quad_state_);
  DCHECK(!shared_quad_state_list_->empty());
  DCHECK(shared_quad_state_list_->back() == current_shared_quad_state_);

  gfx::Rect culled_rect;
  bool has_occlusion_from_outside_target_surface;
  bool impl_draw_transform_is_unknown = false;

  if (for_surface_) {
    culled_rect = occlusion_tracker_.UnoccludedContributingSurfaceContentRect(
        layer_,
        false,
        draw_quad->rect,
        &has_occlusion_from_outside_target_surface);
  } else {
    culled_rect = occlusion_tracker_.UnoccludedContentRect(
        layer_->render_target(),
        draw_quad->rect,
        draw_quad->quadTransform(),
        impl_draw_transform_is_unknown,
        draw_quad->isClipped(),
        draw_quad->clipRect(),
        &has_occlusion_from_outside_target_surface);
  }

  append_quads_data->had_occlusion_from_outside_target_surface |=
      has_occlusion_from_outside_target_surface;

  return AppendQuadInternal(draw_quad.Pass(),
                            culled_rect,
                            quad_list_,
                            occlusion_tracker_,
                            layer_,
                            show_culling_with_debug_border_quads_);
}

}  // namespace cc

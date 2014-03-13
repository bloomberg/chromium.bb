// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/quad_culler.h"

#include "cc/debug/debug_colors.h"
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
                       const OcclusionTracker<LayerImpl>& occlusion_tracker,
                       bool for_surface)
    : quad_list_(quad_list),
      shared_quad_state_list_(shared_quad_state_list),
      layer_(layer),
      occlusion_tracker_(occlusion_tracker),
      current_shared_quad_state_(NULL),
      for_surface_(for_surface) {}

SharedQuadState* QuadCuller::UseSharedQuadState(
    scoped_ptr<SharedQuadState> shared_quad_state) {
  // TODO(danakj): If all quads are culled for the shared_quad_state, we can
  // drop it from the list.
  current_shared_quad_state_ = shared_quad_state.get();
  shared_quad_state_list_->push_back(shared_quad_state.Pass());
  return current_shared_quad_state_;
}

static inline bool AppendQuadInternal(
    scoped_ptr<DrawQuad> draw_quad,
    const gfx::Rect& culled_rect,
    QuadList* quad_list,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    const LayerImpl* layer) {
  bool keep_quad = !culled_rect.IsEmpty();
  if (keep_quad) {
    draw_quad->visible_rect = culled_rect;
    quad_list->push_back(draw_quad.Pass());
  }
  return keep_quad;
}

bool QuadCuller::MaybeAppend(scoped_ptr<DrawQuad> draw_quad) {
  DCHECK(draw_quad->shared_quad_state == current_shared_quad_state_);
  DCHECK(!shared_quad_state_list_->empty());
  DCHECK(shared_quad_state_list_->back() == current_shared_quad_state_);

  gfx::Rect culled_rect;
  if (for_surface_) {
    culled_rect = occlusion_tracker_.UnoccludedContributingSurfaceContentRect(
        layer_, false, draw_quad->visible_rect);
  } else {
    culled_rect =
        occlusion_tracker_.UnoccludedContentRect(layer_->render_target(),
                                                 draw_quad->visible_rect,
                                                 draw_quad->quadTransform());
  }

  return AppendQuadInternal(
      draw_quad.Pass(), culled_rect, quad_list_, occlusion_tracker_, layer_);
}

}  // namespace cc

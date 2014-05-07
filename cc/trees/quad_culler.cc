// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/quad_culler.h"

#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/trees/occlusion_tracker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/transform.h"

namespace cc {

QuadCuller::QuadCuller(RenderPass* render_pass,
                       const LayerImpl* layer,
                       const OcclusionTracker<LayerImpl>& occlusion_tracker)
    : render_pass_(render_pass),
      layer_(layer),
      occlusion_tracker_(occlusion_tracker),
      current_shared_quad_state_(NULL) {
}

SharedQuadState* QuadCuller::CreateSharedQuadState() {
  current_shared_quad_state_ = render_pass_->CreateAndAppendSharedQuadState();
  return current_shared_quad_state_;
}

gfx::Rect QuadCuller::UnoccludedContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  return occlusion_tracker_.UnoccludedContentRect(
      layer_->render_target(), content_rect, draw_transform);
}

gfx::Rect QuadCuller::UnoccludedContributingSurfaceContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  return occlusion_tracker_.UnoccludedContributingSurfaceContentRect(
      layer_, content_rect, draw_transform);
}

void QuadCuller::Append(scoped_ptr<DrawQuad> draw_quad) {
  DCHECK(draw_quad->shared_quad_state == current_shared_quad_state_);
  DCHECK(!render_pass_->shared_quad_state_list.empty());
  DCHECK(render_pass_->shared_quad_state_list.back() ==
         current_shared_quad_state_);
  DCHECK(!draw_quad->rect.IsEmpty());
  DCHECK(!draw_quad->visible_rect.IsEmpty());
  render_pass_->quad_list.push_back(draw_quad.Pass());
}

}  // namespace cc

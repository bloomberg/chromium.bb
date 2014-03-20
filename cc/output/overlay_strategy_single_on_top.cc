// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_single_on_top.h"

#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayCandidateValidator* capability_checker,
    ResourceProvider* resource_provider)
    : capability_checker_(capability_checker),
      resource_provider_(resource_provider) {}

bool OverlayStrategySingleOnTop::Attempt(
    RenderPassList* render_passes_in_draw_order) {
  // Only attempt to handle very simple case for now.
  if (!capability_checker_)
    return false;

  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  DCHECK(root_render_pass);

  QuadList& quad_list = root_render_pass->quad_list;
  const DrawQuad* candidate_quad = quad_list.front();
  if (candidate_quad->material != DrawQuad::TEXTURE_CONTENT)
    return false;

  const TextureDrawQuad& quad = *TextureDrawQuad::MaterialCast(candidate_quad);
  if (!resource_provider_->AllowOverlay(quad.resource_id))
    return false;

  // Simple quads only.
  if (!quad.quadTransform().IsIdentityOrTranslation() || quad.needs_blending ||
      quad.shared_quad_state->opacity != 1.f ||
      quad.shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode ||
      quad.premultiplied_alpha || quad.background_color != SK_ColorTRANSPARENT)
    return false;

  // Add our primary surface.
  OverlayCandidateValidator::OverlayCandidateList candidates;
  OverlayCandidate main_image;
  main_image.display_rect = root_render_pass->output_rect;
  main_image.format = RGBA_8888;
  candidates.push_back(main_image);

  // Add the overlay.
  OverlayCandidate candidate;
  gfx::RectF float_rect(quad.rect);
  quad.quadTransform().TransformRect(&float_rect);
  candidate.transform =
      quad.flipped ? OverlayCandidate::FLIP_VERTICAL : OverlayCandidate::NONE;
  candidate.display_rect = gfx::ToNearestRect(float_rect);
  candidate.uv_rect = BoundingRect(quad.uv_top_left, quad.uv_bottom_right);
  candidate.format = RGBA_8888;
  candidates.push_back(candidate);

  // Check for support.
  capability_checker_->CheckOverlaySupport(&candidates);

  // If the candidate can be handled by an overlay, create a pass for it.
  if (candidates[1].overlay_handled) {
    scoped_ptr<RenderPass> overlay_pass = RenderPass::Create();
    overlay_pass->overlay_state = RenderPass::SIMPLE_OVERLAY;

    scoped_ptr<DrawQuad> overlay_quad = quad_list.take(quad_list.begin());
    quad_list.erase(quad_list.begin());
    overlay_pass->quad_list.push_back(overlay_quad.Pass());
    render_passes_in_draw_order->insert(render_passes_in_draw_order->begin(),
                                        overlay_pass.Pass());
  }
  return candidates[1].overlay_handled;
}

}  // namespace cc

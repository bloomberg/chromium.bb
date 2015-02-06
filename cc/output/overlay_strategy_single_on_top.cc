// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_single_on_top.h"

#include <limits>

#include "cc/quads/draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayCandidateValidator* capability_checker,
    ResourceProvider* resource_provider)
    : capability_checker_(capability_checker),
      resource_provider_(resource_provider) {}

bool OverlayStrategySingleOnTop::IsOverlayQuad(const DrawQuad* draw_quad) {
  unsigned int resource_id;
  switch (draw_quad->material) {
    case DrawQuad::TEXTURE_CONTENT:
      resource_id = TextureDrawQuad::MaterialCast(draw_quad)->resource_id;
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      resource_id = StreamVideoDrawQuad::MaterialCast(draw_quad)->resource_id;
      break;
    default:
      return false;
  }
  return resource_provider_->AllowOverlay(resource_id);
}

bool OverlayStrategySingleOnTop::GetTextureQuadInfo(
    const TextureDrawQuad& quad,
    OverlayCandidate* quad_info) {
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(quad.quadTransform(), quad.flipped);
  if (quad.background_color != SK_ColorTRANSPARENT ||
      quad.premultiplied_alpha ||
      overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;
  quad_info->resource_id = quad.resource_id;
  quad_info->transform = overlay_transform;
  quad_info->uv_rect = BoundingRect(quad.uv_top_left, quad.uv_bottom_right);
  return true;
}

bool OverlayStrategySingleOnTop::GetVideoQuadInfo(
    const StreamVideoDrawQuad& quad,
    OverlayCandidate* quad_info) {
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(quad.quadTransform(), false);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;
  if (!quad.matrix.IsScaleOrTranslation()) {
    // We cannot handle anything other than scaling & translation for texture
    // coordinates yet.
    return false;
  }
  quad_info->resource_id = quad.resource_id;
  quad_info->transform = overlay_transform;

  gfx::Point3F uv0 = gfx::Point3F(0, 0, 0);
  gfx::Point3F uv1 = gfx::Point3F(1, 1, 0);
  quad.matrix.TransformPoint(&uv0);
  quad.matrix.TransformPoint(&uv1);
  gfx::Vector3dF delta = uv1 - uv0;
  if (delta.x() < 0) {
    quad_info->transform = OverlayCandidate::ModifyTransform(
        quad_info->transform, gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL);
    float x0 = uv0.x();
    uv0.set_x(uv1.x());
    uv1.set_x(x0);
    delta.set_x(-delta.x());
  }

  if (delta.y() < 0) {
    // In this situation, uv0y < uv1y. Since we overlay inverted, a request
    // to invert the source texture means we can just output the texture
    // normally and it will be correct.
    quad_info->uv_rect = gfx::RectF(uv0.x(), uv1.y(), delta.x(), -delta.y());
  } else {
    quad_info->transform = OverlayCandidate::ModifyTransform(
        quad_info->transform, gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL);
    quad_info->uv_rect = gfx::RectF(uv0.x(), uv0.y(), delta.x(), delta.y());
  }
  return true;
}

bool OverlayStrategySingleOnTop::GetCandidateQuadInfo(
    const DrawQuad& draw_quad,
    OverlayCandidate* quad_info) {
  // All quad checks.
  if (draw_quad.needs_blending || draw_quad.shared_quad_state->opacity != 1.f ||
      draw_quad.shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode)
    return false;

  if (draw_quad.material == DrawQuad::TEXTURE_CONTENT) {
    const TextureDrawQuad& quad = *TextureDrawQuad::MaterialCast(&draw_quad);
    if (!GetTextureQuadInfo(quad, quad_info))
      return false;
  } else if (draw_quad.material == DrawQuad::STREAM_VIDEO_CONTENT) {
    const StreamVideoDrawQuad& quad =
        *StreamVideoDrawQuad::MaterialCast(&draw_quad);
    if (!GetVideoQuadInfo(quad, quad_info))
      return false;
  }

  quad_info->format = RGBA_8888;
  quad_info->display_rect = OverlayCandidate::GetOverlayRect(
      draw_quad.quadTransform(), draw_quad.rect);
  return true;
}

bool OverlayStrategySingleOnTop::IsInvisibleQuad(const DrawQuad* draw_quad) {
  if (draw_quad->material == DrawQuad::SOLID_COLOR) {
    const SolidColorDrawQuad* solid_quad =
        SolidColorDrawQuad::MaterialCast(draw_quad);
    SkColor color = solid_quad->color;
    float opacity = solid_quad->opacity();
    float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
    // Ignore transparent solid color quads.
    return solid_quad->ShouldDrawWithBlending() &&
           alpha < std::numeric_limits<float>::epsilon();
  }
  return false;
}

bool OverlayStrategySingleOnTop::Attempt(
    RenderPassList* render_passes_in_draw_order,
    OverlayCandidateList* candidate_list) {
  // Only attempt to handle very simple case for now.
  if (!capability_checker_)
    return false;

  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  DCHECK(root_render_pass);

  OverlayCandidate candidate;
  QuadList& quad_list = root_render_pass->quad_list;
  auto candidate_iterator = quad_list.end();
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    const DrawQuad* draw_quad = *it;
    if (IsOverlayQuad(draw_quad)) {
      // Check that no prior quads overlap it.
      bool intersects = false;
      gfx::RectF rect = draw_quad->rect;
      draw_quad->quadTransform().TransformRect(&rect);
      for (auto overlap_iter = quad_list.cbegin(); overlap_iter != it;
           ++overlap_iter) {
        gfx::RectF overlap_rect = overlap_iter->rect;
        overlap_iter->quadTransform().TransformRect(&overlap_rect);
        if (rect.Intersects(overlap_rect) && !IsInvisibleQuad(*overlap_iter)) {
          intersects = true;
          break;
        }
      }
      if (intersects || !GetCandidateQuadInfo(*draw_quad, &candidate))
        continue;
      candidate_iterator = it;
      break;
    }
  }
  if (candidate_iterator == quad_list.end())
    return false;

  // Add our primary surface.
  OverlayCandidateList candidates;
  OverlayCandidate main_image;
  main_image.display_rect = root_render_pass->output_rect;
  candidates.push_back(main_image);

  // Add the overlay.
  candidate.plane_z_order = 1;
  candidates.push_back(candidate);

  // Check for support.
  capability_checker_->CheckOverlaySupport(&candidates);

  // If the candidate can be handled by an overlay, create a pass for it.
  if (candidates[1].overlay_handled) {
    quad_list.EraseAndInvalidateAllPointers(candidate_iterator);
    candidate_list->swap(candidates);
    return true;
  }
  return false;
}

}  // namespace cc

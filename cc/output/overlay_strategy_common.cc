// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_common.h"

#include <limits>

#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayStrategyCommon::OverlayStrategyCommon() {
}

OverlayStrategyCommon::~OverlayStrategyCommon() {
}

bool OverlayStrategyCommon::IsOverlayQuad(const DrawQuad* draw_quad) {
  switch (draw_quad->material) {
    case DrawQuad::TEXTURE_CONTENT:
      return TextureDrawQuad::MaterialCast(draw_quad)->allow_overlay();
    case DrawQuad::STREAM_VIDEO_CONTENT:
      return StreamVideoDrawQuad::MaterialCast(draw_quad)->allow_overlay();
    default:
      return false;
  }
}

bool OverlayStrategyCommon::IsInvisibleQuad(const DrawQuad* draw_quad) {
  if (draw_quad->material == DrawQuad::SOLID_COLOR) {
    const SolidColorDrawQuad* solid_quad =
        SolidColorDrawQuad::MaterialCast(draw_quad);
    SkColor color = solid_quad->color;
    float opacity = solid_quad->shared_quad_state->opacity;
    float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
    return solid_quad->ShouldDrawWithBlending() &&
           alpha < std::numeric_limits<float>::epsilon();
  }
  return false;
}

bool OverlayStrategyCommon::GetTextureQuadInfo(const TextureDrawQuad& quad,
                                               OverlayCandidate* quad_info) {
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(
          quad.shared_quad_state->quad_to_target_transform, quad.y_flipped);
  if (quad.background_color != SK_ColorTRANSPARENT ||
      quad.premultiplied_alpha ||
      overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;
  quad_info->resource_id = quad.resource_id();
  quad_info->resource_size_in_pixels = quad.resource_size_in_pixels();
  quad_info->transform = overlay_transform;
  quad_info->uv_rect = BoundingRect(quad.uv_top_left, quad.uv_bottom_right);
  return true;
}

bool OverlayStrategyCommon::GetVideoQuadInfo(const StreamVideoDrawQuad& quad,
                                             OverlayCandidate* quad_info) {
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(
          quad.shared_quad_state->quad_to_target_transform, false);
  if (overlay_transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;
  if (!quad.matrix.IsScaleOrTranslation()) {
    // We cannot handle anything other than scaling & translation for texture
    // coordinates yet.
    return false;
  }
  quad_info->resource_id = quad.resource_id();
  quad_info->resource_size_in_pixels = quad.resource_size_in_pixels();
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

bool OverlayStrategyCommon::GetCandidateQuadInfo(const DrawQuad& draw_quad,
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
      draw_quad.shared_quad_state->quad_to_target_transform, draw_quad.rect);
  return true;
}

}  // namespace cc

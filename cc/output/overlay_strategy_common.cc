// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_common.h"

#include <limits>

#include "cc/base/math_util.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayStrategyCommon::OverlayStrategyCommon(
    OverlayCandidateValidator* capability_checker,
    OverlayStrategyCommonDelegate* delegate)
    : capability_checker_(capability_checker), delegate_(delegate) {}

OverlayStrategyCommon::~OverlayStrategyCommon() {
}

bool OverlayStrategyCommon::Attempt(RenderPassList* render_passes_in_draw_order,
                                    OverlayCandidateList* candidate_list,
                                    float device_scale_factor) {
  if (!capability_checker_)
    return false;
  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  DCHECK(root_render_pass);

  // Add our primary surface.
  OverlayCandidate main_image;
  main_image.display_rect = gfx::RectF(root_render_pass->output_rect);
  DCHECK(candidate_list->empty());
  candidate_list->push_back(main_image);

  bool created_overlay = false;
  QuadList& quad_list = root_render_pass->quad_list;
  for (auto it = quad_list.begin(); it != quad_list.end();) {
    OverlayCandidate candidate;
    if (!GetCandidateQuadInfo(**it, &candidate)) {
      ++it;
      continue;
    }

    OverlayResult result = delegate_->TryOverlay(
        capability_checker_, render_passes_in_draw_order, candidate_list,
        candidate, &it, device_scale_factor);
    switch (result) {
      case DID_NOT_CREATE_OVERLAY:
        ++it;
        break;
      case CREATED_OVERLAY_STOP_LOOKING:
        return true;
      case CREATED_OVERLAY_KEEP_LOOKING:
        created_overlay = true;
        break;
    }
  }

  if (!created_overlay) {
    DCHECK_EQ(1u, candidate_list->size());
    candidate_list->clear();
  }

  return created_overlay;
}

// static
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
  if (!quad.allow_overlay())
    return false;
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
  if (!quad.allow_overlay())
    return false;
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

bool OverlayStrategyCommon::GetIOSurfaceQuadInfo(const IOSurfaceDrawQuad& quad,
                                                 OverlayCandidate* quad_info) {
  if (!quad.allow_overlay)
    return false;
  gfx::OverlayTransform overlay_transform =
      OverlayCandidate::GetOverlayTransform(
          quad.shared_quad_state->quad_to_target_transform, false);
  if (overlay_transform != gfx::OVERLAY_TRANSFORM_NONE)
    return false;
  quad_info->resource_id = quad.io_surface_resource_id();
  quad_info->resource_size_in_pixels = quad.io_surface_size;
  quad_info->transform = overlay_transform;
  quad_info->uv_rect = gfx::RectF(1.f, 1.f);
  return true;
}

bool OverlayStrategyCommon::GetCandidateQuadInfo(const DrawQuad& draw_quad,
                                                 OverlayCandidate* quad_info) {
  // All quad checks.
  if (draw_quad.needs_blending || draw_quad.shared_quad_state->opacity != 1.f ||
      draw_quad.shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode)
    return false;

  switch (draw_quad.material) {
    case DrawQuad::TEXTURE_CONTENT: {
      auto* quad = TextureDrawQuad::MaterialCast(&draw_quad);
      if (!GetTextureQuadInfo(*quad, quad_info))
        return false;
    } break;
    case DrawQuad::STREAM_VIDEO_CONTENT: {
      auto* quad = StreamVideoDrawQuad::MaterialCast(&draw_quad);
      if (!GetVideoQuadInfo(*quad, quad_info))
        return false;
    } break;
    case DrawQuad::IO_SURFACE_CONTENT: {
      auto* quad = IOSurfaceDrawQuad::MaterialCast(&draw_quad);
      if (!GetIOSurfaceQuadInfo(*quad, quad_info))
        return false;
    } break;
    default:
      return false;
  }

  quad_info->format = RGBA_8888;
  quad_info->display_rect = OverlayCandidate::GetOverlayRect(
      draw_quad.shared_quad_state->quad_to_target_transform, draw_quad.rect);
  quad_info->quad_rect_in_target_space = MathUtil::MapEnclosingClippedRect(
      draw_quad.shared_quad_state->quad_to_target_transform, draw_quad.rect);
  quad_info->clip_rect = draw_quad.shared_quad_state->clip_rect;
  quad_info->is_clipped = draw_quad.shared_quad_state->is_clipped;
  return true;
}

}  // namespace cc

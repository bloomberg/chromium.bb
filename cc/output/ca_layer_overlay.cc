// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/ca_layer_overlay.h"

#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/resource_provider.h"

namespace cc {

namespace {

bool FromIOSurfaceQuad(ResourceProvider* resource_provider,
                       const IOSurfaceDrawQuad* quad,
                       CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->io_surface_resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);
  return true;
}

bool FromStreamVideoQuad(ResourceProvider* resource_provider,
                         const StreamVideoDrawQuad* quad,
                         CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;
  ca_layer_overlay->contents_resource_id = resource_id;
  // TODO(ccameron): Support merging at least some basic transforms into the
  // layer transform.
  if (!quad->matrix.IsIdentity())
    return false;
  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);
  return true;
}

bool FromSolidColorDrawQuad(const SolidColorDrawQuad* quad,
                            CALayerOverlay* ca_layer_overlay,
                            bool* skip) {
  // Do not generate quads that are completely transparent.
  if (SkColorGetA(quad->color) == 0) {
    *skip = true;
    return true;
  }
  ca_layer_overlay->background_color = quad->color;
  return true;
}

bool FromTextureQuad(ResourceProvider* resource_provider,
                     const TextureDrawQuad* quad,
                     CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;
  // TODO(ccameron): Merge the y flip into the layer transform.
  if (quad->y_flipped)
    return false;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  ca_layer_overlay->background_color = quad->background_color;
  return true;
}

bool FromTileQuad(ResourceProvider* resource_provider,
                  const TileDrawQuad* quad,
                  CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return false;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = quad->tex_coord_rect;
  ca_layer_overlay->contents_rect.Scale(1.f / quad->texture_size.width(),
                                        1.f / quad->texture_size.height());
  return true;
}

bool FromDrawQuad(ResourceProvider* resource_provider,
                  const gfx::RectF& display_rect,
                  const DrawQuad* quad,
                  CALayerOverlay* ca_layer_overlay,
                  bool* skip) {
  if (quad->shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode)
    return false;

  // TODO(ccameron): Handle 3D transforms.
  if (!quad->shared_quad_state->quad_to_target_transform.IsFlat())
    return false;

  // Early-out for invisible quads.
  if (quad->shared_quad_state->opacity == 0.f) {
    *skip = true;
    return true;
  }

  // Check rect clipping.
  gfx::RectF quad_rect(quad->rect);
  if (quad->shared_quad_state->is_clipped) {
    gfx::RectF clip_rect = gfx::RectF(quad->shared_quad_state->clip_rect);
    gfx::RectF quad_rect_in_clip_space = gfx::RectF(quad->rect);
    quad->shared_quad_state->quad_to_target_transform.TransformRect(
        &quad_rect_in_clip_space);
    quad_rect_in_clip_space.Intersect(display_rect);
    // Skip quads that are entirely clipped.
    if (!quad_rect_in_clip_space.Intersects(clip_rect)) {
      *skip = true;
      return true;
    }
    // Fall back if the clip rect actually has an effect.
    // TODO(ccameron): Handle more clip rects.
    if (!clip_rect.Contains(quad_rect_in_clip_space)) {
      return false;
    }
  }

  ca_layer_overlay->opacity = quad->shared_quad_state->opacity;
  ca_layer_overlay->bounds_size = gfx::SizeF(quad->rect.size());
  ca_layer_overlay->transform.setTranslate(quad->rect.x(), quad->rect.y(), 0);
  ca_layer_overlay->transform.postConcat(
      quad->shared_quad_state->quad_to_target_transform.matrix());

  switch (quad->material) {
    case DrawQuad::IO_SURFACE_CONTENT:
      return FromIOSurfaceQuad(resource_provider,
                               IOSurfaceDrawQuad::MaterialCast(quad),
                               ca_layer_overlay);
    case DrawQuad::TEXTURE_CONTENT:
      return FromTextureQuad(resource_provider,
                             TextureDrawQuad::MaterialCast(quad),
                             ca_layer_overlay);
    case DrawQuad::TILED_CONTENT:
      return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                          ca_layer_overlay);
    case DrawQuad::SOLID_COLOR:
      return FromSolidColorDrawQuad(SolidColorDrawQuad::MaterialCast(quad),
                                    ca_layer_overlay, skip);
    case DrawQuad::STREAM_VIDEO_CONTENT:
      return FromStreamVideoQuad(resource_provider,
                                 StreamVideoDrawQuad::MaterialCast(quad),
                                 ca_layer_overlay);
    default:
      return false;
      break;
  }

  return true;
}

}  // namespace

CALayerOverlay::CALayerOverlay()
    : contents_resource_id(0),
      opacity(1),
      background_color(SK_ColorTRANSPARENT),
      transform(SkMatrix44::kIdentity_Constructor) {}

CALayerOverlay::~CALayerOverlay() {}

bool ProcessForCALayerOverlays(ResourceProvider* resource_provider,
                               const gfx::RectF& display_rect,
                               const QuadList& quad_list,
                               CALayerOverlayList* ca_layer_overlays) {
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad* quad = *it;
    CALayerOverlay ca_layer_overlay;
    bool skip = false;
    if (!FromDrawQuad(resource_provider, display_rect, quad, &ca_layer_overlay,
                      &skip)) {
      ca_layer_overlays->clear();
      return false;
    }
    if (skip)
      continue;
    ca_layer_overlays->push_back(ca_layer_overlay);
  }
  return true;
}

}  // namespace cc

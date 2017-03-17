// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/dc_layer_overlay.h"

#include "cc/base/math_util.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace {

DCLayerOverlayProcessor::DCLayerResult FromYUVQuad(
    ResourceProvider* resource_provider,
    const YUVVideoDrawQuad* quad,
    DCLayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->y_plane_resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return DCLayerOverlayProcessor::DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = quad->ya_tex_coord_rect;
  ca_layer_overlay->filter = GL_LINEAR;
  return DCLayerOverlayProcessor::DC_LAYER_SUCCESS;
}

// Find a rectangle containing all the quads in a list that occlude the area
// in target_quad.
gfx::RectF GetOcclusionBounds(const gfx::RectF& target_quad,
                              QuadList::ConstIterator quad_list_begin,
                              QuadList::ConstIterator quad_list_end) {
  gfx::RectF occlusion_bounding_box;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    gfx::RectF overlap_rect = MathUtil::MapClippedRect(
        overlap_iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(overlap_iter->rect));
    float opacity = overlap_iter->shared_quad_state->opacity;
    if (opacity < std::numeric_limits<float>::epsilon())
      continue;
    const DrawQuad* quad = *overlap_iter;
    if (quad->material == DrawQuad::SOLID_COLOR) {
      SkColor color = SolidColorDrawQuad::MaterialCast(quad)->color;
      float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
      if (quad->ShouldDrawWithBlending() &&
          alpha < std::numeric_limits<float>::epsilon())
        continue;
    }
    overlap_rect.Intersect(target_quad);
    if (!overlap_rect.IsEmpty()) {
      occlusion_bounding_box.Union(overlap_rect);
    }
  }
  return occlusion_bounding_box;
}

}  // namespace

DCLayerOverlay::DCLayerOverlay() : filter(GL_LINEAR) {}

DCLayerOverlay::DCLayerOverlay(const DCLayerOverlay& other) = default;

DCLayerOverlay::~DCLayerOverlay() {}

DCLayerOverlayProcessor::DCLayerResult DCLayerOverlayProcessor::FromDrawQuad(
    ResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    QuadList::ConstIterator quad_list_begin,
    QuadList::ConstIterator quad,
    DCLayerOverlay* ca_layer_overlay) {
  if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
    return DC_LAYER_FAILED_QUAD_BLEND_MODE;

  DCLayerResult result = DC_LAYER_FAILED_UNKNOWN;
  switch (quad->material) {
    case DrawQuad::YUV_VIDEO_CONTENT:
      result =
          FromYUVQuad(resource_provider, YUVVideoDrawQuad::MaterialCast(*quad),
                      ca_layer_overlay);
      break;
    default:
      return DC_LAYER_FAILED_UNKNOWN;
  }
  if (result != DC_LAYER_SUCCESS)
    return result;

  scoped_refptr<DCLayerOverlaySharedState> overlay_shared_state(
      new DCLayerOverlaySharedState);
  overlay_shared_state->z_order = 1;

  overlay_shared_state->is_clipped = quad->shared_quad_state->is_clipped;
  overlay_shared_state->clip_rect =
      gfx::RectF(quad->shared_quad_state->clip_rect);

  overlay_shared_state->opacity = quad->shared_quad_state->opacity;
  overlay_shared_state->transform =
      quad->shared_quad_state->quad_to_target_transform.matrix();

  ca_layer_overlay->shared_state = overlay_shared_state;
  ca_layer_overlay->bounds_rect = gfx::RectF(quad->rect);

  return result;
}

void DCLayerOverlayProcessor::Process(ResourceProvider* resource_provider,
                                      const gfx::RectF& display_rect,
                                      QuadList* quad_list,
                                      gfx::Rect* overlay_damage_rect,
                                      gfx::Rect* damage_rect,
                                      DCLayerOverlayList* ca_layer_overlays) {
  gfx::Rect this_frame_underlay_rect;
  bool display_rect_changed = (display_rect != previous_display_rect_);
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    DCLayerOverlay ca_layer;
    DCLayerResult result = FromDrawQuad(resource_provider, display_rect,
                                        quad_list->begin(), it, &ca_layer);
    if (result != DC_LAYER_SUCCESS)
      continue;
    gfx::Rect quad_rectangle = MathUtil::MapEnclosingClippedRect(
        it->shared_quad_state->quad_to_target_transform, it->rect);
    gfx::RectF occlusion_bounding_box =
        GetOcclusionBounds(gfx::RectF(quad_rectangle), quad_list->begin(), it);
    overlay_damage_rect->Union(quad_rectangle);

    if (occlusion_bounding_box.IsEmpty()) {
      // The quad is on top, so promote it to an overlay and remove all damage
      // underneath it.
      if (it->shared_quad_state->quad_to_target_transform
              .Preserves2dAxisAlignment() &&
          !display_rect_changed) {
        damage_rect->Subtract(quad_rectangle);
      }
      quad_list->EraseAndInvalidateAllPointers(it);
    } else {
      // The quad is occluded, so replace it with a black solid color quad and
      // place the overlay itself under the quad.
      if (it->shared_quad_state->quad_to_target_transform
              .IsIdentityOrIntegerTranslation()) {
        this_frame_underlay_rect = quad_rectangle;
      }
      ca_layer.shared_state->z_order = -1;
      const SharedQuadState* shared_quad_state = it->shared_quad_state;
      gfx::Rect rect = it->visible_rect;
      SolidColorDrawQuad* replacement =
          quad_list->ReplaceExistingElement<SolidColorDrawQuad>(it);
      replacement->SetAll(shared_quad_state, rect, rect, rect, false,
                          SK_ColorTRANSPARENT, true);

      if (this_frame_underlay_rect == previous_frame_underlay_rect_) {
        // If this underlay rect is the same as for last frame, subtract its
        // area from the damage of the main surface, as the cleared area was
        // already cleared last frame. Add back the damage from the occluded
        // area for this and last frame, as that may have changed.
        if (it->shared_quad_state->quad_to_target_transform
                .Preserves2dAxisAlignment() &&
            !display_rect_changed) {
          gfx::Rect occluding_damage_rect = *damage_rect;
          occluding_damage_rect.Intersect(quad_rectangle);
          damage_rect->Subtract(quad_rectangle);
          gfx::Rect new_occlusion_bounding_box =
              gfx::ToEnclosingRect(occlusion_bounding_box);
          new_occlusion_bounding_box.Union(previous_occlusion_bounding_box_);
          occluding_damage_rect.Intersect(new_occlusion_bounding_box);

          damage_rect->Union(occluding_damage_rect);
        }
      } else {
        // Entire replacement quad must be redrawn.
        damage_rect->Union(quad_rectangle);
      }
      previous_occlusion_bounding_box_ =
          gfx::ToEnclosingRect(occlusion_bounding_box);
    }

    ca_layer_overlays->push_back(ca_layer);
    // Only allow one overlay for now.
    break;
  }
  damage_rect->Intersect(gfx::ToEnclosingRect(display_rect));
  previous_frame_underlay_rect_ = this_frame_underlay_rect;
  previous_display_rect_ = display_rect;
}

}  // namespace cc

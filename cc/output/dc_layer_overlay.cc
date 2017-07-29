// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/dc_layer_overlay.h"

#include "base/metrics/histogram_macros.h"
#include "cc/base/math_util.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_switches.h"

namespace cc {

namespace {

DCLayerOverlayProcessor::DCLayerResult FromYUVQuad(
    ResourceProvider* resource_provider,
    const YUVVideoDrawQuad* quad,
    DCLayerOverlay* ca_layer_overlay) {
  for (const auto& resource : quad->resources) {
    if (!resource_provider->IsOverlayCandidate(resource))
      return DCLayerOverlayProcessor::DC_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  }
  ca_layer_overlay->resources = quad->resources;
  ca_layer_overlay->contents_rect = quad->ya_tex_coord_rect;
  ca_layer_overlay->filter = GL_LINEAR;
  ca_layer_overlay->color_space = quad->video_color_space;
  return DCLayerOverlayProcessor::DC_LAYER_SUCCESS;
}

// This returns the smallest rectangle in target space that contains the quad.
gfx::RectF ClippedQuadRectangle(const DrawQuad* quad) {
  gfx::RectF quad_rect = MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->rect));
  if (quad->shared_quad_state->is_clipped)
    quad_rect.Intersect(gfx::RectF(quad->shared_quad_state->clip_rect));
  return quad_rect;
}

// Find a rectangle containing all the quads in a list that occlude the area
// in target_quad.
gfx::RectF GetOcclusionBounds(const gfx::RectF& target_quad,
                              QuadList::ConstIterator quad_list_begin,
                              QuadList::ConstIterator quad_list_end) {
  gfx::RectF occlusion_bounding_box;
  for (auto overlap_iter = quad_list_begin; overlap_iter != quad_list_end;
       ++overlap_iter) {
    float opacity = overlap_iter->shared_quad_state->opacity;
    if (opacity < std::numeric_limits<float>::epsilon())
      continue;
    const DrawQuad* quad = *overlap_iter;
    gfx::RectF overlap_rect = ClippedQuadRectangle(quad);
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

void RecordDCLayerResult(DCLayerOverlayProcessor::DCLayerResult result) {
  UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.DCLayerResult", result,
                            DCLayerOverlayProcessor::DC_LAYER_FAILED_MAX);
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

  DCLayerResult result;
  switch (quad->material) {
    case DrawQuad::YUV_VIDEO_CONTENT:
      result =
          FromYUVQuad(resource_provider, YUVVideoDrawQuad::MaterialCast(*quad),
                      ca_layer_overlay);
      break;
    default:
      return DC_LAYER_FAILED_UNSUPPORTED_QUAD;
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
                                      RenderPassList* render_passes,
                                      gfx::Rect* overlay_damage_rect,
                                      gfx::Rect* damage_rect,
                                      DCLayerOverlayList* ca_layer_overlays) {
  ProcessRenderPass(resource_provider, display_rect,
                    render_passes->back().get(), true, overlay_damage_rect,
                    damage_rect, ca_layer_overlays);
}

void DCLayerOverlayProcessor::ProcessRenderPass(
    ResourceProvider* resource_provider,
    const gfx::RectF& display_rect,
    RenderPass* render_pass,
    bool is_root,
    gfx::Rect* overlay_damage_rect,
    gfx::Rect* damage_rect,
    DCLayerOverlayList* ca_layer_overlays) {
  gfx::Rect this_frame_underlay_rect;
  QuadList* quad_list = &render_pass->quad_list;
  for (auto it = quad_list->begin(); it != quad_list->end(); ++it) {
    DCLayerOverlay dc_layer;
    DCLayerResult result = FromDrawQuad(resource_provider, display_rect,
                                        quad_list->begin(), it, &dc_layer);
    if (result != DC_LAYER_SUCCESS) {
      RecordDCLayerResult(result);
      continue;
    }

    if (!it->shared_quad_state->quad_to_target_transform
             .Preserves2dAxisAlignment() &&
        !base::FeatureList::IsEnabled(
            features::kDirectCompositionComplexOverlays)) {
      RecordDCLayerResult(DC_LAYER_FAILED_COMPLEX_TRANSFORM);
      continue;
    }

    dc_layer.shared_state->transform.postConcat(
        render_pass->transform_to_root_target.matrix());

    gfx::Rect quad_rectangle = gfx::ToEnclosingRect(ClippedQuadRectangle(*it));
    gfx::RectF occlusion_bounding_box =
        GetOcclusionBounds(gfx::RectF(quad_rectangle), quad_list->begin(), it);

    // Underlays are less efficient, so attempt regular overlays first.
    if ((is_root &&
         ProcessForOverlay(display_rect, quad_list, quad_rectangle,
                           occlusion_bounding_box, it, damage_rect)) ||
        ProcessForUnderlay(display_rect, render_pass, quad_rectangle,
                           occlusion_bounding_box, it, is_root, damage_rect,
                           &this_frame_underlay_rect, &dc_layer)) {
      overlay_damage_rect->Union(quad_rectangle);

      RecordDCLayerResult(DC_LAYER_SUCCESS);
      ca_layer_overlays->push_back(dc_layer);
      // Only allow one overlay for now.
      break;
    }
  }
  damage_rect->Intersect(gfx::ToEnclosingRect(display_rect));
  previous_frame_underlay_rect_ = this_frame_underlay_rect;
  previous_display_rect_ = display_rect;
}

bool DCLayerOverlayProcessor::ProcessForOverlay(
    const gfx::RectF& display_rect,
    QuadList* quad_list,
    const gfx::Rect& quad_rectangle,
    const gfx::RectF& occlusion_bounding_box,
    const QuadList::Iterator& it,
    gfx::Rect* damage_rect) {
  bool display_rect_changed = (display_rect != previous_display_rect_);
  if (!occlusion_bounding_box.IsEmpty())
    return false;
  // The quad is on top, so promote it to an overlay and remove all damage
  // underneath it.
  if (it->shared_quad_state->quad_to_target_transform
          .Preserves2dAxisAlignment() &&
      !display_rect_changed && !it->ShouldDrawWithBlending()) {
    damage_rect->Subtract(quad_rectangle);
  }
  quad_list->EraseAndInvalidateAllPointers(it);
  return true;
}

bool DCLayerOverlayProcessor::ProcessForUnderlay(
    const gfx::RectF& display_rect,
    RenderPass* render_pass,
    const gfx::Rect& quad_rectangle,
    const gfx::RectF& occlusion_bounding_box,
    const QuadList::Iterator& it,
    bool is_root,
    gfx::Rect* damage_rect,
    gfx::Rect* this_frame_underlay_rect,
    DCLayerOverlay* dc_layer) {
  if (!base::FeatureList::IsEnabled(features::kDirectCompositionUnderlays)) {
    RecordDCLayerResult(DC_LAYER_FAILED_OCCLUDED);
    return false;
  }
  if (!is_root) {
    RecordDCLayerResult(DC_LAYER_FAILED_NON_ROOT);
    return false;
  }
  if ((it->shared_quad_state->opacity < 1.0)) {
    RecordDCLayerResult(DC_LAYER_FAILED_TRANSPARENT);
    return false;
  }
  bool display_rect_changed = (display_rect != previous_display_rect_);
  // The quad is occluded, so replace it with a black solid color quad and
  // place the overlay itself under the quad.
  if (it->shared_quad_state->quad_to_target_transform
          .IsIdentityOrIntegerTranslation()) {
    *this_frame_underlay_rect = quad_rectangle;
  }
  dc_layer->shared_state->z_order = -1;
  const SharedQuadState* shared_quad_state = it->shared_quad_state;
  gfx::Rect rect = it->visible_rect;
  SolidColorDrawQuad* replacement =
      render_pass->quad_list.ReplaceExistingElement<SolidColorDrawQuad>(it);
  replacement->SetAll(shared_quad_state, rect, rect, rect, false,
                      SK_ColorTRANSPARENT, true);

  if (*this_frame_underlay_rect == previous_frame_underlay_rect_ && is_root) {
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
  return true;
}

}  // namespace cc

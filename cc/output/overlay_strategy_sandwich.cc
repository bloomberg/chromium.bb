// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_sandwich.h"

#include "cc/base/region.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

gfx::Rect AlignPixelRectToDIP(float scale_factor, const gfx::Rect& pixel_rect) {
  gfx::Rect dip_rect =
      gfx::ToEnclosingRect(gfx::ScaleRect(pixel_rect, 1.0f / scale_factor));
  gfx::Rect new_pixel_rect =
      gfx::ToEnclosingRect(gfx::ScaleRect(dip_rect, scale_factor));
  return new_pixel_rect;
}

bool IsPixelRectAlignedToDIP(float scale_factor, const gfx::Rect& pixel_rect) {
  return (pixel_rect == AlignPixelRectToDIP(scale_factor, pixel_rect));
}

}  // namespace

namespace cc {

OverlayStrategySandwich::~OverlayStrategySandwich() {}

bool OverlayStrategySandwich::TryOverlay(
    OverlayCandidateValidator* capability_checker,
    RenderPassList* render_passes_in_draw_order,
    OverlayCandidateList* output_candidate_list,
    const OverlayCandidate& candidate,
    QuadList::Iterator candidate_iter_in_quad_list,
    float device_scale_factor) {
  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  QuadList& quad_list = root_render_pass->quad_list;
  gfx::Rect pixel_bounds = root_render_pass->output_rect;

  const DrawQuad* candidate_quad = *candidate_iter_in_quad_list;
  const gfx::Transform& candidate_transform =
      candidate_quad->shared_quad_state->quad_to_target_transform;
  gfx::Transform candidate_inverse_transform;
  if (!candidate_transform.GetInverse(&candidate_inverse_transform))
    return false;

  // Compute the candidate's rect in display space (pixels on the screen). The
  // rect needs to be DIP-aligned, or we cannot use it.
  gfx::RectF candidate_pixel_rect_float = candidate_quad->rect;
  candidate_transform.TransformRect(&candidate_pixel_rect_float);
  gfx::Rect candidate_pixel_rect;
  candidate_pixel_rect = gfx::ToEnclosingRect(candidate_pixel_rect_float);
  if (candidate_pixel_rect != candidate_pixel_rect_float)
    return false;
  if (!IsPixelRectAlignedToDIP(device_scale_factor, candidate_pixel_rect))
    return false;

  // Iterate through the quads in front of |potential_candidate|, and compute
  // the region of |potential_candidate| that is covered.
  Region pixel_covered_region;
  for (auto overlap_iter = quad_list.cbegin();
       overlap_iter != candidate_iter_in_quad_list; ++overlap_iter) {
    if (OverlayStrategyCommon::IsInvisibleQuad(*overlap_iter))
      continue;
    // Compute the quad's bounds in display space, and ensure that it is rounded
    // up to be DIP-aligned.
    gfx::RectF pixel_covered_rect_float = overlap_iter->rect;
    overlap_iter->shared_quad_state->quad_to_target_transform.TransformRect(
        &pixel_covered_rect_float);
    gfx::Rect pixel_covered_rect = AlignPixelRectToDIP(
        device_scale_factor, gfx::ToEnclosingRect(pixel_covered_rect_float));

    // Include the intersection of that quad with the candidate's quad in the
    // covered region.
    pixel_covered_rect.Intersect(candidate_pixel_rect);
    pixel_covered_region.Union(pixel_covered_rect);
  }

  // Add our primary surface.
  OverlayCandidateList new_candidate_list;
  OverlayCandidate main_image;
  main_image.display_rect = pixel_bounds;
  new_candidate_list.push_back(main_image);

  // Add the candidate's overlay.
  DCHECK(candidate.resource_id);
  new_candidate_list.push_back(candidate);
  new_candidate_list.back().plane_z_order = 1;

  // Add an overlay of the primary surface for any part of the candidate's
  // quad that was covered.
  // TODO(ccameron): Create an OverlayCandidate for each rect in the region.
  gfx::Rect pixel_covered_rect = pixel_covered_region.bounds();
  DCHECK(IsPixelRectAlignedToDIP(device_scale_factor, pixel_covered_rect));
  if (!pixel_covered_rect.IsEmpty()) {
    OverlayCandidate main_image_on_top;
    main_image_on_top.display_rect = pixel_covered_rect;
    main_image_on_top.uv_rect = pixel_covered_rect;
    main_image_on_top.uv_rect.Scale(1.f / pixel_bounds.width(),
                                    1.f / pixel_bounds.height());
    main_image_on_top.plane_z_order = 2;
    main_image_on_top.transform = gfx::OVERLAY_TRANSFORM_NONE;
    main_image_on_top.use_output_surface_for_resource = true;
    new_candidate_list.push_back(main_image_on_top);
  }

  // Check for support.
  capability_checker->CheckOverlaySupport(&new_candidate_list);
  for (const OverlayCandidate& candidate : new_candidate_list) {
    if (candidate.plane_z_order > 0 && !candidate.overlay_handled)
      return false;
  }

  // Remove the quad for the overlay quad. Replace it with a transparent quad
  // if we're putting a new overlay on top.
  if (pixel_covered_rect.IsEmpty()) {
    quad_list.EraseAndInvalidateAllPointers(candidate_iter_in_quad_list);
  } else {
    gfx::RectF quad_space_covered_rect_float = pixel_covered_rect;
    candidate_inverse_transform.TransformRect(&quad_space_covered_rect_float);
    gfx::Rect quad_space_covered_rect =
        gfx::ToEnclosingRect(quad_space_covered_rect_float);
    quad_space_covered_rect.Intersect(candidate_quad->rect);

    const SharedQuadState* shared_quad_state =
        candidate_quad->shared_quad_state;

    SolidColorDrawQuad* replacement =
        quad_list.ReplaceExistingElement<SolidColorDrawQuad>(
            candidate_iter_in_quad_list);
    replacement->SetAll(shared_quad_state, quad_space_covered_rect,
                        quad_space_covered_rect, quad_space_covered_rect, false,
                        SK_ColorTRANSPARENT, true);
  }

  output_candidate_list->swap(new_candidate_list);
  return true;
}

}  // namespace cc

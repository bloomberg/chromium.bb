// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_single_on_top.h"

#include <limits>

#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"

namespace cc {

OverlayStrategySingleOnTop::OverlayStrategySingleOnTop(
    OverlayCandidateValidator* capability_checker)
    : capability_checker_(capability_checker) {
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
      draw_quad->shared_quad_state->quad_to_target_transform.TransformRect(
          &rect);
      for (auto overlap_iter = quad_list.cbegin(); overlap_iter != it;
           ++overlap_iter) {
        gfx::RectF overlap_rect = overlap_iter->rect;
        overlap_iter->shared_quad_state->quad_to_target_transform.TransformRect(
            &overlap_rect);
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

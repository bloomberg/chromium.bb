// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_underlay.h"

#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"

namespace cc {

OverlayStrategyUnderlay::OverlayStrategyUnderlay(
    OverlayCandidateValidator* capability_checker)
    : OverlayStrategyCommon(capability_checker) {
}

bool OverlayStrategyUnderlay::TryOverlay(
    OverlayCandidateValidator* capability_checker,

    RenderPassList* render_passes_in_draw_order,
    OverlayCandidateList* candidate_list,
    const OverlayCandidate& candidate,
    QuadList::Iterator candidate_iterator) {
  RenderPass* root_render_pass = render_passes_in_draw_order->back();
  QuadList& quad_list = root_render_pass->quad_list;

  // Add our primary surface.
  OverlayCandidateList candidates;
  OverlayCandidate main_image;
  main_image.display_rect = root_render_pass->output_rect;
  candidates.push_back(main_image);

  // Add the overlay.
  candidates.push_back(candidate);
  candidates.back().plane_z_order = -1;

  // Check for support.
  capability_checker->CheckOverlaySupport(&candidates);

  // If the candidate can be handled by an overlay, create a pass for it. We
  // need to switch out the video quad with a black transparent one.
  if (candidates[1].overlay_handled) {
    const SharedQuadState* shared_quad_state =
        candidate_iterator->shared_quad_state;
    gfx::Rect rect = candidate_iterator->visible_rect;
    SolidColorDrawQuad* replacement =
        quad_list.ReplaceExistingElement<SolidColorDrawQuad>(
            candidate_iterator);
    replacement->SetAll(shared_quad_state, rect, rect, rect, false,
                        SK_ColorTRANSPARENT, true);
    candidate_list->swap(candidates);
    return true;
  }
  return false;
}

}  // namespace cc

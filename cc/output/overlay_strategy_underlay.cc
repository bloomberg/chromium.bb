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
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategyUnderlay::~OverlayStrategyUnderlay() {}

bool OverlayStrategyUnderlay::Attempt(
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  QuadList& quad_list = render_pass->quad_list;
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    OverlayCandidate candidate;
    if (!OverlayCandidate::FromDrawQuad(resource_provider, *it, &candidate))
      continue;

    // Add the overlay.
    OverlayCandidateList new_candidate_list = *candidate_list;
    new_candidate_list.push_back(candidate);
    new_candidate_list.back().plane_z_order = -1;

    // Check for support.
    capability_checker_->CheckOverlaySupport(&new_candidate_list);

    // If the candidate can be handled by an overlay, create a pass for it. We
    // need to switch out the video quad with a black transparent one.
    if (new_candidate_list.back().overlay_handled) {
      new_candidate_list.back().is_unoccluded =
          !OverlayCandidate::IsOccluded(candidate, quad_list.cbegin(), it);
      quad_list.ReplaceExistingQuadWithOpaqueTransparentSolidColor(it);
      candidate_list->swap(new_candidate_list);

      // This quad will be promoted.  We clear the promotable hints here, since
      // we can only promote a single quad.  Otherwise, somebody might try to
      // back one of the promotable quads with a SurfaceView, and either it or
      // |candidate| would have to fall back to a texture.
      candidate_list->promotion_hint_info_map_.clear();
      candidate_list->AddPromotionHint(candidate);
      return true;
    } else {
      // If |candidate| should get a promotion hint, then rememeber that now.
      candidate_list->promotion_hint_info_map_.insert(
          new_candidate_list.promotion_hint_info_map_.begin(),
          new_candidate_list.promotion_hint_info_map_.end());
    }
  }

  return false;
}

}  // namespace cc

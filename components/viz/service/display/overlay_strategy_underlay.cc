// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay.h"

#include "build/build_config.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_candidate_validator.h"

namespace viz {

OverlayStrategyUnderlay::OverlayStrategyUnderlay(
    OverlayCandidateValidator* capability_checker,
    OpaqueMode opaque_mode)
    : capability_checker_(capability_checker), opaque_mode_(opaque_mode) {
  DCHECK(capability_checker);
}

OverlayStrategyUnderlay::~OverlayStrategyUnderlay() {}

bool OverlayStrategyUnderlay::Attempt(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  QuadList& quad_list = render_pass->quad_list;
  const bool compute_hints =
      resource_provider->DoAnyResourcesWantPromotionHints();

  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    OverlayCandidate candidate;

    // If we're computing the list of all resources that requested promotion
    // hints, then add this candidate to the set if needed.
    if (compute_hints) {
      candidate_list->AddToPromotionHintRequestorSetIfNeeded(resource_provider,
                                                             *it);
    }

    if (!OverlayCandidate::FromDrawQuad(resource_provider, output_color_matrix,
                                        *it, &candidate) ||
        (opaque_mode_ == OpaqueMode::RequireOpaqueCandidates &&
         !candidate.is_opaque)) {
      continue;
    }

    // Filters read back the framebuffer to get the pixel values that need to
    // be filtered.  This is a problem when there are hardware planes because
    // the planes are not composited until they are on the display controller.
    if (OverlayCandidate::IsOccludedByFilteredQuad(
            candidate, quad_list.begin(), it, render_pass_backdrop_filters)) {
      continue;
    }

    // Add the overlay.
    OverlayCandidateList new_candidate_list = *candidate_list;
    new_candidate_list.push_back(candidate);
    new_candidate_list.back().plane_z_order = -1;
    new_candidate_list.front().is_opaque = false;

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
      if (compute_hints) {
        // Finish the quad list to find any other resources.
        for (; it != quad_list.end(); ++it) {
          candidate_list->AddToPromotionHintRequestorSetIfNeeded(
              resource_provider, *it);
        }
      }
      return true;
    } else {
      // If |candidate| should get a promotion hint, then remember that now.
      candidate_list->promotion_hint_info_map_.insert(
          new_candidate_list.promotion_hint_info_map_.begin(),
          new_candidate_list.promotion_hint_info_map_.end());
    }
  }

  return false;
}

OverlayProcessor::StrategyType OverlayStrategyUnderlay::GetUMAEnum() const {
  return OverlayProcessor::StrategyType::kUnderlay;
}

}  // namespace viz

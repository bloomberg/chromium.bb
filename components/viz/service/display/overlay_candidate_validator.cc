// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_validator.h"

#include "base/metrics/histogram_macros.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayCandidateValidator::OverlayCandidateValidator() = default;
OverlayCandidateValidator::~OverlayCandidateValidator() = default;

bool OverlayCandidateValidator::AttemptWithStrategies(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_pass_list,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) const {
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_pass_list, candidates,
                          content_bounds)) {
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                                strategy->GetUMAEnum());
      return true;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                            OverlayStrategy::kNoStrategyUsed);
  return false;
}

gfx::Rect OverlayCandidateValidator::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  return gfx::ToEnclosedRect(candidate.display_rect);
}

}  // namespace viz

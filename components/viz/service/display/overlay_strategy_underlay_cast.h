// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_

#include "base/macros.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class OverlayCandidateValidator;

// Similar to underlay strategy plus Cast-specific handling of content bounds.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlayCast
    : public OverlayStrategyUnderlay {
 public:
  explicit OverlayStrategyUnderlayCast(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyUnderlayCast() override;

  bool Attempt(const SkMatrix44& output_color_matrix,
               cc::DisplayResourceProvider* resource_provider,
               RenderPass* render_pass,
               cc::OverlayCandidateList* candidate_list,
               std::vector<gfx::Rect>* content_bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyUnderlayCast);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_STRATEGY_UNDERLAY_CAST_H_

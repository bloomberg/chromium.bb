// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class OverlayCandidatesOzone;
}

namespace viz {

class VIZ_SERVICE_EXPORT CompositorOverlayCandidateValidatorOzone
    : public CompositorOverlayCandidateValidator {
 public:
  CompositorOverlayCandidateValidatorOzone(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::vector<OverlayStrategy> strategies);
  ~CompositorOverlayCandidateValidatorOzone() override;

  // OverlayCandidateValidator implementation.
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() override;
  bool AllowDCLayerOverlays() override;
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override;

  // CompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  const std::vector<OverlayStrategy> strategies_;
  bool software_mirror_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorOzone);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

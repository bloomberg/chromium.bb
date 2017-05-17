// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_
#define COMPONENTS_VIZ_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/viz/viz_export.h"

namespace viz {

class VIZ_EXPORT CompositorOverlayCandidateValidatorMac
    : public CompositorOverlayCandidateValidator {
 public:
  explicit CompositorOverlayCandidateValidatorMac(bool ca_layer_disabled);
  ~CompositorOverlayCandidateValidatorMac() override;

  // cc::OverlayCandidateValidator implementation.
  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() override;
  bool AllowDCLayerOverlays() override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;

  // CompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  bool software_mirror_active_;
  const bool ca_layer_disabled_;

  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorMac);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

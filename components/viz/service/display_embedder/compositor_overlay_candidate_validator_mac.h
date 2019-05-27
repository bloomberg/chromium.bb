// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT CompositorOverlayCandidateValidatorMac
    : public CompositorOverlayCandidateValidator {
 public:
  explicit CompositorOverlayCandidateValidatorMac(bool ca_layer_disabled);
  ~CompositorOverlayCandidateValidatorMac() override;

  // OverlayCandidateValidator implementation.
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override;

  // CompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  const bool ca_layer_disabled_;

  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorMac);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

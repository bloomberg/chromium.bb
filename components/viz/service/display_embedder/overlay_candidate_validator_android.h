// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_

#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayCandidateValidatorAndroid
    : public OverlayCandidateValidator {
 public:
  OverlayCandidateValidatorAndroid();
  ~OverlayCandidateValidatorAndroid() override;

  void GetStrategies(OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() const override;
  bool AllowDCLayerOverlays() const override;
  bool NeedsSurfaceOccludingDamageRect() const override;
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override;
  void AdjustOutputSurfaceOverlay(OverlayCandidate* candidate) override;

  void SetDisplayTransform(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& viewport_size) override;

 private:
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;

  DISALLOW_COPY_AND_ASSIGN(OverlayCandidateValidatorAndroid);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_

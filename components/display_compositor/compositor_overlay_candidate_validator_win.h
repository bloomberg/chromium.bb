// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_

#include "base/macros.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/display_compositor_export.h"

namespace display_compositor {

// This is a simple overlay candidate validator that promotes everything
// possible to an overlay.
class DISPLAY_COMPOSITOR_EXPORT CompositorOverlayCandidateValidatorWin
    : public CompositorOverlayCandidateValidator {
 public:
  CompositorOverlayCandidateValidatorWin();
  ~CompositorOverlayCandidateValidatorWin() override;

  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;
  bool AllowCALayerOverlays() override;

  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorWin);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_WIN_H_

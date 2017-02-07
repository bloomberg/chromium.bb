// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/display_compositor_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class OverlayCandidatesOzone;
}

namespace display_compositor {

class DISPLAY_COMPOSITOR_EXPORT CompositorOverlayCandidateValidatorOzone
    : public CompositorOverlayCandidateValidator {
 public:
  CompositorOverlayCandidateValidatorOzone(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::string strategies_string);
  ~CompositorOverlayCandidateValidatorOzone() override;

  // cc::OverlayCandidateValidator implementation.
  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;

  // CompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  // Callback declaration to allocate a new OverlayProcessor::Strategy.
  using StrategyInstantiator =
      base::Callback<std::unique_ptr<cc::OverlayProcessor::Strategy>(
          CompositorOverlayCandidateValidatorOzone*)>;
  // List callbacks used to instantiate OverlayProcessor::Strategy
  // as defined by |strategies_string| paramter in the constructor.
  std::vector<StrategyInstantiator> strategies_instantiators_;
  bool software_mirror_active_;

  DISALLOW_COPY_AND_ASSIGN(CompositorOverlayCandidateValidatorOzone);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

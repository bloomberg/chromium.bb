// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class CONTENT_EXPORT BrowserCompositorOverlayCandidateValidatorMac
    : public BrowserCompositorOverlayCandidateValidator {
 public:
  explicit BrowserCompositorOverlayCandidateValidatorMac(
      bool ca_layer_disabled);
  ~BrowserCompositorOverlayCandidateValidatorMac() override;

  // cc::OverlayCandidateValidator implementation.
  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  bool AllowCALayerOverlays() override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;

  // BrowserCompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  gfx::AcceleratedWidget widget_;
  bool software_mirror_active_;
  const bool ca_layer_disabled_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOverlayCandidateValidatorMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_MAC_H_

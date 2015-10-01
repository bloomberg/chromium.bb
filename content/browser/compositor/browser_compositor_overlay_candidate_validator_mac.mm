// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_overlay_candidate_validator_mac.h"

#include "cc/output/overlay_strategy_sandwich.h"

namespace content {

BrowserCompositorOverlayCandidateValidatorMac::
    BrowserCompositorOverlayCandidateValidatorMac(
        gfx::AcceleratedWidget widget)
    : widget_(widget),
      software_mirror_active_(false) {
}

BrowserCompositorOverlayCandidateValidatorMac::
    ~BrowserCompositorOverlayCandidateValidatorMac() {
}

void BrowserCompositorOverlayCandidateValidatorMac::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
  strategies->push_back(scoped_ptr<cc::OverlayProcessor::Strategy>(
      new cc::OverlayStrategyCommon(this, new cc::OverlayStrategySandwich)));
}

void BrowserCompositorOverlayCandidateValidatorMac::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
  // SW mirroring copies out of the framebuffer, so we can't remove any
  // quads for overlaying, otherwise the output is incorrect.
  if (software_mirror_active_)
    return;

  for (size_t i = 0; i < surfaces->size(); ++i)
    surfaces->at(i).overlay_handled = true;
}

void BrowserCompositorOverlayCandidateValidatorMac::SetSoftwareMirrorMode(
    bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace content

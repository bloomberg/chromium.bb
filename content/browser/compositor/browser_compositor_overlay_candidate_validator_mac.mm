// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_overlay_candidate_validator_mac.h"

#include <stddef.h>

namespace content {

BrowserCompositorOverlayCandidateValidatorMac::
    BrowserCompositorOverlayCandidateValidatorMac(bool ca_layer_disabled)
    : software_mirror_active_(false), ca_layer_disabled_(ca_layer_disabled) {}

BrowserCompositorOverlayCandidateValidatorMac::
    ~BrowserCompositorOverlayCandidateValidatorMac() {
}

void BrowserCompositorOverlayCandidateValidatorMac::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
}

bool BrowserCompositorOverlayCandidateValidatorMac::AllowCALayerOverlays() {
  return !ca_layer_disabled_ && !software_mirror_active_;
}

void BrowserCompositorOverlayCandidateValidatorMac::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
}

void BrowserCompositorOverlayCandidateValidatorMac::SetSoftwareMirrorMode(
    bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_overlay_candidate_validator_mac.h"

#include <stddef.h>

#include "cc/output/overlay_strategy_sandwich.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"

namespace content {

BrowserCompositorOverlayCandidateValidatorMac::
    BrowserCompositorOverlayCandidateValidatorMac(
        gfx::AcceleratedWidget widget)
    : widget_(widget),
      software_mirror_active_(false),
      ca_layers_disabled_(
          GpuDataManagerImpl::GetInstance()->IsDriverBugWorkaroundActive(
              gpu::DISABLE_OVERLAY_CA_LAYERS)) {
}

BrowserCompositorOverlayCandidateValidatorMac::
    ~BrowserCompositorOverlayCandidateValidatorMac() {
}

void BrowserCompositorOverlayCandidateValidatorMac::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
}

bool BrowserCompositorOverlayCandidateValidatorMac::AllowCALayerOverlays() {
  if (software_mirror_active_ || ca_layers_disabled_)
    return false;
  return true;
}

void BrowserCompositorOverlayCandidateValidatorMac::CheckOverlaySupport(
    cc::OverlayCandidateList* surfaces) {
  // SW mirroring copies out of the framebuffer, so we can't remove any
  // quads for overlaying, otherwise the output is incorrect.
  if (software_mirror_active_)
    return;

  if (ca_layers_disabled_)
    return;

  for (size_t i = 0; i < surfaces->size(); ++i)
    surfaces->at(i).overlay_handled = true;
}

void BrowserCompositorOverlayCandidateValidatorMac::SetSoftwareMirrorMode(
    bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace content

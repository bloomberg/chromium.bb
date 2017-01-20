// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/compositor_overlay_candidate_validator_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace display_compositor {

CompositorOverlayCandidateValidatorWin::
    CompositorOverlayCandidateValidatorWin() {}

CompositorOverlayCandidateValidatorWin::
    ~CompositorOverlayCandidateValidatorWin() {}

void CompositorOverlayCandidateValidatorWin::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
  strategies->push_back(base::MakeUnique<cc::OverlayStrategyUnderlay>(this));
}

void CompositorOverlayCandidateValidatorWin::CheckOverlaySupport(
    cc::OverlayCandidateList* candidates) {
  for (cc::OverlayCandidate& candidate : *candidates) {
    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;
  }
}

bool CompositorOverlayCandidateValidatorWin::AllowCALayerOverlays() {
  return false;
}

void CompositorOverlayCandidateValidatorWin::SetSoftwareMirrorMode(
    bool enabled) {
  // Software mirroring isn't supported on Windows.
  NOTIMPLEMENTED();
}

}  // namespace display_compositor

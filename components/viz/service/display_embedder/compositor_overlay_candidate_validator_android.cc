// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

CompositorOverlayCandidateValidatorAndroid::
    CompositorOverlayCandidateValidatorAndroid() {}

CompositorOverlayCandidateValidatorAndroid::
    ~CompositorOverlayCandidateValidatorAndroid() {}

void CompositorOverlayCandidateValidatorAndroid::GetStrategies(
    cc::OverlayProcessor::StrategyList* strategies) {
  strategies->push_back(base::MakeUnique<cc::OverlayStrategyUnderlay>(this));
}

void CompositorOverlayCandidateValidatorAndroid::CheckOverlaySupport(
    cc::OverlayCandidateList* candidates) {
  // There should only be at most a single overlay candidate: the video quad.
  // There's no check that the presented candidate is really a video frame for
  // a fullscreen video. Instead it's assumed that if a quad is marked as
  // overlayable, it's a fullscreen video quad.
  DCHECK_LE(candidates->size(), 1u);

  if (!candidates->empty()) {
    cc::OverlayCandidate& candidate = candidates->front();

    // This quad either will be promoted, or would be if it were backed by a
    // SurfaceView.  Record that it should get a promotion hint.
    candidates->AddPromotionHint(candidate);

    if (candidate.is_backed_by_surface_texture) {
      // This quad would be promoted if it were backed by a SurfaceView.  Since
      // it isn't, we can't promote it.
      return;
    }

    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;
  }
}

bool CompositorOverlayCandidateValidatorAndroid::AllowCALayerOverlays() {
  return false;
}

bool CompositorOverlayCandidateValidatorAndroid::AllowDCLayerOverlays() {
  return false;
}

// Overlays will still be allowed when software mirroring is enabled, even
// though they won't appear in the mirror.
void CompositorOverlayCandidateValidatorAndroid::SetSoftwareMirrorMode(
    bool enabled) {}

}  // namespace viz

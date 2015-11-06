// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_processor.h"

#include "cc/output/output_surface.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

OverlayProcessor::OverlayProcessor(OutputSurface* surface) : surface_(surface) {
}

void OverlayProcessor::Initialize() {
  DCHECK(surface_);
  OverlayCandidateValidator* validator =
      surface_->GetOverlayCandidateValidator();
  if (validator)
    validator->GetStrategies(&strategies_);
}

OverlayProcessor::~OverlayProcessor() {}

void OverlayProcessor::ProcessForOverlays(ResourceProvider* resource_provider,
                                          RenderPassList* render_passes,
                                          OverlayCandidateList* candidates,
                                          gfx::Rect* damage_rect) {
  for (auto strategy : strategies_) {
    if (strategy->Attempt(resource_provider, render_passes, candidates,
                          damage_rect)) {
      return;
    }
  }
}

}  // namespace cc

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_processor.h"

#include "cc/output/output_surface.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/quads/draw_quad.h"
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

bool OverlayProcessor::ProcessForCALayers(
    ResourceProvider* resource_provider,
    RenderPassList* render_passes,
    CALayerOverlayList* ca_layer_overlays,
    OverlayCandidateList* overlay_candidates) {
  RenderPass* root_render_pass = render_passes->back().get();

  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(root_render_pass->output_rect),
          root_render_pass->quad_list, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then clear the list and remove the backbuffer from the overcandidate
  // list.
  overlay_candidates->clear();
  render_passes->back()->quad_list.clear();
  return true;
}

void OverlayProcessor::ProcessForOverlays(ResourceProvider* resource_provider,
                                          RenderPassList* render_passes,
                                          OverlayCandidateList* candidates,
                                          gfx::Rect* damage_rect) {
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(resource_provider, render_passes, candidates,
                          damage_rect)) {
      return;
    }
  }
}

}  // namespace cc

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_fullscreen.h"

#include "cc/base/math_util.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

OverlayStrategyFullscreen::OverlayStrategyFullscreen() {}

OverlayStrategyFullscreen::~OverlayStrategyFullscreen() {}

bool OverlayStrategyFullscreen::Attempt(ResourceProvider* resource_provider,
                                        RenderPass* render_pass,
                                        OverlayCandidateList* candidate_list) {
  QuadList* quad_list = &render_pass->quad_list;
  // First quad of quad_list is the top most quad.
  auto front = quad_list->begin();
  while (front != quad_list->end()) {
    if (!OverlayCandidate::IsInvisibleQuad(*front))
      break;
    front++;
  }

  if (front == quad_list->end())
    return false;

  OverlayCandidate candidate;
  if (!OverlayCandidate::FromDrawQuad(resource_provider, *front, &candidate)) {
    return false;
  }

  if (candidate.transform != gfx::OVERLAY_TRANSFORM_NONE) {
    return false;
  }

  if (!candidate.display_rect.origin().IsOrigin() ||
      gfx::ToRoundedSize(candidate.display_rect.size()) !=
          render_pass->output_rect.size() ||
      render_pass->output_rect.size() != candidate.resource_size_in_pixels) {
    return false;
  }

  candidate.plane_z_order = 1;
  candidate.overlay_handled = true;
  candidate_list->push_back(candidate);
  quad_list->EraseAndInvalidateAllPointers(quad_list->begin());
  return true;
}

}  // namespace cc

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay_cast.h"

#include "base/containers/adapters.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayStrategyUnderlayCast::OverlayStrategyUnderlayCast(
    OverlayCandidateValidator* capability_checker)
    : OverlayStrategyUnderlay(capability_checker) {}

OverlayStrategyUnderlayCast::~OverlayStrategyUnderlayCast() {}

bool OverlayStrategyUnderlayCast::Attempt(
    const SkMatrix44& output_color_matrix,
    cc::DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    cc::OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  const QuadList& const_quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(const_quad_list)) {
    if (cc::OverlayCandidate::IsInvisibleQuad(quad))
      continue;

    const auto& transform = quad->shared_quad_state->quad_to_target_transform;
    gfx::RectF quad_rect = gfx::RectF(quad->rect);
    transform.TransformRect(&quad_rect);

    bool is_underlay = false;
    if (!found_underlay) {
      cc::OverlayCandidate candidate;
      is_underlay = cc::OverlayCandidate::FromDrawQuad(
          resource_provider, output_color_matrix, quad, &candidate);
      found_underlay = is_underlay;
    }

    if (!found_underlay && quad->material == DrawQuad::SOLID_COLOR) {
      const SolidColorDrawQuad* solid = SolidColorDrawQuad::MaterialCast(quad);
      if (solid->color == SK_ColorBLACK)
        continue;
    }

    if (is_underlay) {
      content_rect.Subtract(gfx::ToEnclosedRect(quad_rect));
    } else {
      content_rect.Union(gfx::ToEnclosingRect(quad_rect));
    }
  }

  const bool result = OverlayStrategyUnderlay::Attempt(
      output_color_matrix, resource_provider, render_pass, candidate_list,
      content_bounds);
  DCHECK(content_bounds && content_bounds->empty());
  DCHECK(result == found_underlay);
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return result;
}

}  // namespace viz

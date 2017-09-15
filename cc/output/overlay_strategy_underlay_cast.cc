// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_strategy_underlay_cast.h"

#include "base/containers/adapters.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

OverlayStrategyUnderlayCast::OverlayStrategyUnderlayCast(
    OverlayCandidateValidator* capability_checker)
    : OverlayStrategyUnderlay(capability_checker) {}

OverlayStrategyUnderlayCast::~OverlayStrategyUnderlayCast() {}

bool OverlayStrategyUnderlayCast::Attempt(
    DisplayResourceProvider* resource_provider,
    viz::RenderPass* render_pass,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  const viz::QuadList& const_quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(const_quad_list)) {
    if (OverlayCandidate::IsInvisibleQuad(quad))
      continue;

    const auto& transform = quad->shared_quad_state->quad_to_target_transform;
    gfx::RectF quad_rect = gfx::RectF(quad->rect);
    transform.TransformRect(&quad_rect);

    bool is_underlay = false;
    if (!found_underlay) {
      OverlayCandidate candidate;
      is_underlay =
          OverlayCandidate::FromDrawQuad(resource_provider, quad, &candidate);
      found_underlay = is_underlay;
    }

    if (!found_underlay && quad->material == viz::DrawQuad::SOLID_COLOR) {
      const viz::SolidColorDrawQuad* solid =
          viz::SolidColorDrawQuad::MaterialCast(quad);
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
      resource_provider, render_pass, candidate_list, content_bounds);
  DCHECK(content_bounds && content_bounds->empty());
  DCHECK(result == found_underlay);
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return result;
}

}  // namespace cc

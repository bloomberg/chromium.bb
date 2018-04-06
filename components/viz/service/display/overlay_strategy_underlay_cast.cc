// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay_cast.h"

#include "base/containers/adapters.h"
#include "base/lazy_instance.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

base::LazyInstance<OverlayStrategyUnderlayCast::OverlayCompositedCallback>::
    DestructorAtExit g_overlay_composited_callback = LAZY_INSTANCE_INITIALIZER;

}  // namespace

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
  QuadList& quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(quad_list)) {
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

  if (found_underlay) {
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      cc::OverlayCandidate candidate;
      if (!cc::OverlayCandidate::FromDrawQuad(
              resource_provider, output_color_matrix, *it, &candidate)) {
        continue;
      }

      render_pass->quad_list.ReplaceExistingQuadWithOpaqueTransparentSolidColor(
          it);

      if (!g_overlay_composited_callback.Get().is_null()) {
        g_overlay_composited_callback.Get().Run(candidate.display_rect,
                                                candidate.transform);
      }

      break;
    }
  }

  DCHECK(content_bounds && content_bounds->empty());
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return found_underlay;
}

// static
void OverlayStrategyUnderlayCast::SetOverlayCompositedCallback(
    const OverlayCompositedCallback& cb) {
  g_overlay_composited_callback.Get() = cb;
}

}  // namespace viz

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_simple_bounds.h"

namespace viz {

namespace {

bool ContainsSurface(const CompositorFrame& compositor_frame) {
  for (const auto& render_pass : compositor_frame.render_pass_list) {
    for (const DrawQuad* quad : render_pass->quad_list) {
      if (quad->material == DrawQuad::SURFACE_CONTENT)
        return true;
    }
  }
  return false;
}

}  // namespace

HitTestDataProviderSimpleBounds::HitTestDataProviderSimpleBounds() = default;
HitTestDataProviderSimpleBounds::~HitTestDataProviderSimpleBounds() = default;

mojom::HitTestRegionListPtr HitTestDataProviderSimpleBounds::GetHitTestData(
    const CompositorFrame& compositor_frame) const {
  // Derive hit test regions from information in the CompositorFrame.
  auto hit_test_region_list = mojom::HitTestRegionList::New();

  // Use kHitTestAsk only when there is an embedded Surface(OOPIF).
  // This code will evolve over time so that we only use the kHitTestAsk flag
  // when necessary.
  hit_test_region_list->flags =
      mojom::kHitTestMouse | mojom::kHitTestTouch |
      (ContainsSurface(compositor_frame) ? mojom::kHitTestAsk
                                         : mojom::kHitTestMine);
  hit_test_region_list->bounds = gfx::Rect(compositor_frame.size_in_pixels());
  return hit_test_region_list;
}

}  // namespace viz

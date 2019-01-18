// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_draw_quad.h"

#include "components/viz/common/hit_test/hit_test_data_builder.h"

namespace viz {

HitTestDataProviderDrawQuad::HitTestDataProviderDrawQuad(
    bool should_ask_for_child_region,
    bool root_accepts_events)
    : should_ask_for_child_region_(should_ask_for_child_region),
      root_accepts_events_(root_accepts_events) {}

HitTestDataProviderDrawQuad::~HitTestDataProviderDrawQuad() = default;

// Derives HitTestRegions from information in the |compositor_frame|.
base::Optional<HitTestRegionList> HitTestDataProviderDrawQuad::GetHitTestData(
    const CompositorFrame& compositor_frame) const {
  return HitTestDataBuilder::CreateHitTestData(
      compositor_frame, root_accepts_events_, should_ask_for_child_region_);
}

}  // namespace viz

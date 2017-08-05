// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

#include <stdint.h>

#include "components/viz/common/surfaces/surface_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace viz {

// A AggregatedHitTestRegion element with child_count of kEndOfList indicates
// the last element and end of the list.
constexpr int32_t kEndOfList = -1;

// An array of AggregatedHitTestRegion elements is used to define the
// aggregated hit-test data for the Display.
//
// It is designed to be in shared memory so that the viz service can
// write the hit_test data, and the viz host can read without
// process hops.
struct AggregatedHitTestRegion {
  AggregatedHitTestRegion(FrameSinkId frame_sink_id,
                          uint32_t flags,
                          gfx::Rect rect,
                          gfx::Transform transform,
                          int32_t child_count)
      : frame_sink_id(frame_sink_id),
        flags(flags),
        rect(rect),
        transform(transform),
        child_count(child_count) {}

  // The FrameSinkId corresponding to this region.  Events that match
  // are routed to this surface.
  FrameSinkId frame_sink_id;

  // Flags to indicate the type of region as defined for
  // mojom::HitTestRegion
  uint32_t flags;

  // The rectangle that defines the region in parent region's coordinate space.
  gfx::Rect rect;

  // The transform applied to the rect in parent region's coordinate space.
  gfx::Transform transform;

  // The number of children including their children below this entry.
  // If this element is not matched then child_count elements can be skipped
  // to move to the next entry.
  int32_t child_count;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

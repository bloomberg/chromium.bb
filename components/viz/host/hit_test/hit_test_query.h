// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_
#define COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_

#include <vector>

#include "base/macros.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/geometry/point.h"

namespace viz {

struct Target {
  FrameSinkId frame_sink_id;
  // Coordinates in the coordinate system of the target FrameSinkId.
  gfx::Point location_in_target;
  // Different flags are defined in services/viz/public/interfaces/hit_test/
  // hit_test_region_list.mojom.
  uint32_t flags = 0;
};

enum class EventSource {
  MOUSE,
  TOUCH,
};

// Finds the target for a given location based on the AggregatedHitTestRegion
// list aggregated by HitTestAggregator.
// TODO(riajiang): Handle 3d space cases correctly.
class VIZ_HOST_EXPORT HitTestQuery {
 public:
  HitTestQuery();
  ~HitTestQuery();

  // TODO(riajiang): Need to validate the data received.
  // http://crbug.com/746470
  // HitTestAggregator should only send new active_handle and idle_handle when
  // they are initialized or replaced with OnAggregatedHitTestRegionListUpdated.
  // Both handles must be valid. HitTestQuery would store and update these two
  // handles received.
  // HitTestAggregator would tell HitTestQuery to update its active hit test
  // list based on |active_handle_index| with
  // SwitchActiveAggregatedHitTestRegionList if HitTestAggregator only swapped
  // handles.
  void OnAggregatedHitTestRegionListUpdated(
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size);
  void SwitchActiveAggregatedHitTestRegionList(uint8_t active_handle_index);

  // Finds Target for |location_in_root|, including the FrameSinkId of the
  // target, updated location in the coordinate system of the target and
  // hit-test flags for the target.
  // Assumptions about the AggregatedHitTestRegion list received.
  // 1. The list is in ascending (front to back) z-order.
  // 2. Children count includes children of children.
  // 3. After applying transform to the incoming point, point is in the same
  // coordinate system as the bounds it is comparing against.
  // For example,
  //  +e-------------+
  //  |   +c---------|
  //  | 1 |+a--+     |
  //  |   || 2 |     |
  //  |   |+b--------|
  //  |   ||         |
  //  |   ||   3     |
  //  +--------------+
  // In this case, after applying identity transform, 1 is in the coordinate
  // system of e; apply the transfrom-from-e-to-c and transform-from-c-to-a
  // then we get 2 in the coordinate system of a; apply the
  // transfrom-from-e-to-c and transform-from-c-to-b then we get 3 in the
  // coordinate system of b.
  Target FindTargetForLocation(EventSource event_source,
                               const gfx::Point& location_in_root) const;

  // When a target window is already known, e.g. capture/latched window, convert
  // |location_in_root| to be in the coordinate space of the target.
  // |target_ancestors| contains the FrameSinkId from target to root.
  // |target_ancestors.front()| is the target, and |target_ancestors.back()|
  // is the root.
  gfx::Point TransformLocationForTarget(
      EventSource event_source,
      const std::vector<FrameSinkId>& target_ancestors,
      const gfx::Point& location_in_root) const;

 private:
  // Helper function to find |target| for |location_in_parent| in the |region|,
  // returns true if a target is found and false otherwise. |location_in_parent|
  // is in the coordinate space of |region|'s parent.
  bool FindTargetInRegionForLocation(EventSource event_source,
                                     const gfx::Point& location_in_parent,
                                     AggregatedHitTestRegion* region,
                                     Target* target) const;

  // Transform |location_in_target| to be in |region|'s coordinate space.
  // |location_in_target| is in the coordinate space of |region|'s parent at the
  // beginning.
  bool TransformLocationForTargetRecursively(
      EventSource event_source,
      const std::vector<FrameSinkId>& target_ancestors,
      size_t target_ancestor,
      AggregatedHitTestRegion* region,
      gfx::Point* location_in_target) const;

  uint32_t handle_buffer_sizes_[2];
  mojo::ScopedSharedBufferMapping handle_buffers_[2];

  AggregatedHitTestRegion* active_hit_test_list_ = nullptr;
  uint32_t active_hit_test_list_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(HitTestQuery);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_

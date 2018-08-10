// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_
#define COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/host/viz_host_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {
class HitTestRegionObserver;
}

namespace viz {

struct Target {
  FrameSinkId frame_sink_id;
  // Coordinates in the coordinate system of the target FrameSinkId.
  gfx::PointF location_in_target;
  // Different flags are defined in services/viz/public/interfaces/hit_test/
  // hit_test_region_list.mojom.
  uint32_t flags = 0;
};

enum class EventSource {
  MOUSE,
  TOUCH,
  ANY,
};

// Finds the target for a given location based on the AggregatedHitTestRegion
// list aggregated by HitTestAggregator.
// TODO(riajiang): Handle 3d space cases correctly.
class VIZ_HOST_EXPORT HitTestQuery {
 public:
  explicit HitTestQuery(
      base::RepeatingClosure shut_down_gpu_callback = base::RepeatingClosure());
  ~HitTestQuery();

  // TODO(riajiang): Need to validate the data received.
  // http://crbug.com/746470
  // HitTestAggregator has sent the most recent |hit_test_data| for targeting/
  // transforming requests.
  void OnAggregatedHitTestRegionListUpdated(
      const std::vector<AggregatedHitTestRegion>& hit_test_data);

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
                               const gfx::PointF& location_in_root) const;

  // When a target window is already known, e.g. capture/latched window, convert
  // |location_in_root| to be in the coordinate space of the target and store
  // that in |transformed_location|. Return true if the transform is successful
  // and false otherwise.
  // |target_ancestors| contains the FrameSinkId from target to root.
  // |target_ancestors.front()| is the target, and |target_ancestors.back()|
  // is the root.
  bool TransformLocationForTarget(
      EventSource event_source,
      const std::vector<FrameSinkId>& target_ancestors,
      const gfx::PointF& location_in_root,
      gfx::PointF* transformed_location) const;

  // Gets the transform from root to |target| in physical pixels. Returns true
  // and stores the result into |transform| if successful, returns false
  // otherwise. This is potentially a little more expensive than
  // TransformLocationForTarget(). So if the path from root to target is known,
  // then that is the preferred API.
  bool GetTransformToTarget(const FrameSinkId& target,
                            gfx::Transform* transform) const;

  // Returns whether hit test data for |frame_sink_id| is available.
  bool ContainsFrameSinkId(const FrameSinkId& frame_sink_id) const;

 private:
  friend class content::HitTestRegionObserver;
  // Helper function to find |target| for |location_in_parent| in the
  // |region_index|, returns true if a target is found and false otherwise.
  // |location_in_parent| is in the coordinate space of |region_index|'s parent.
  bool FindTargetInRegionForLocation(EventSource event_source,
                                     const gfx::PointF& location_in_parent,
                                     uint32_t region_index,
                                     Target* target) const;

  // Transform |location_in_target| to be in |region_index|'s coordinate space.
  // |location_in_target| is in the coordinate space of |region_index|'s parent
  // at the beginning.
  bool TransformLocationForTargetRecursively(
      EventSource event_source,
      const std::vector<FrameSinkId>& target_ancestors,
      size_t target_ancestor,
      uint32_t region_index,
      gfx::PointF* location_in_target) const;

  bool GetTransformToTargetRecursively(const FrameSinkId& target,
                                       uint32_t region_index,
                                       gfx::Transform* transform) const;

  void ReceivedBadMessageFromGpuProcess() const;

  std::vector<AggregatedHitTestRegion> hit_test_data_;
  uint32_t hit_test_data_size_ = 0;

  // Log bad message and shut down Viz process when it is compromised.
  base::RepeatingClosure bad_message_gpu_callback_;

  DISALLOW_COPY_AND_ASSIGN(HitTestQuery);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HIT_TEST_HIT_TEST_QUERY_H_

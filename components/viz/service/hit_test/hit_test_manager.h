// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_MANAGER_H_

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"

namespace viz {

class FrameSinkManagerImpl;

// HitTestManager manages the collection of HitTestRegionList objects
// submitted in calls to SubmitCompositorFrame.  This collection is
// used by HitTestAggregator.
class VIZ_SERVICE_EXPORT HitTestManager : public SurfaceObserver {
 public:
  explicit HitTestManager(FrameSinkManagerImpl* frame_sink_manager);
  virtual ~HitTestManager();

  // Called when HitTestRegionList is submitted along with every call
  // to SubmitCompositorFrame.
  void SubmitHitTestRegionList(
      const SurfaceId& surface_id,
      const uint64_t frame_index,
      mojom::HitTestRegionListPtr hit_test_region_list);

  // Returns the HitTestRegionList corresponding to the given
  // surface_id and the active CompositorFrame matched by frame_index.
  const mojom::HitTestRegionList* GetActiveHitTestRegionList(
      const SurfaceId& surface_id) const;

  // SurfaceObserver:
  void OnSurfaceCreated(const SurfaceId& surface_id) override {}
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}
  void OnSurfaceActivated(const SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override {}
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override {}

 private:
  friend class TestHitTestManager;

  bool ValidateHitTestRegionList(
      const SurfaceId& surface_id,
      const mojom::HitTestRegionListPtr& hit_test_region_list);
  bool ValidateHitTestRegion(const SurfaceId& surface_id,
                             const mojom::HitTestRegionPtr& hit_test_region);

  FrameSinkManagerImpl* const frame_sink_manager_;

  std::map<SurfaceId, base::flat_map<uint64_t, mojom::HitTestRegionListPtr>>
      hit_test_region_lists_;

  DISALLOW_COPY_AND_ASSIGN(HitTestManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_MANAGER_H_

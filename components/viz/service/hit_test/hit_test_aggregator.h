// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_

#include "cc/surfaces/surface_observer.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/hit_test/public/interfaces/hit_test_region_list.mojom.h"

namespace viz {

// HitTestAggregator collects HitTestRegionList objects from surfaces and
// aggregates them into a DisplayHitTesData structue made available in
// shared memory to enable efficient hit testing across processes.
//
// This is intended to be created in the viz or GPU process. For mus+ash this
// will be true after the mus process split.
class VIZ_SERVICE_EXPORT HitTestAggregator : public cc::SurfaceObserver {
 public:
  HitTestAggregator();
  ~HitTestAggregator();

  // Called when HitTestRegionList is submitted along with every call
  // to SubmitCompositorFrame.  This is collected in pending_ until
  // surfaces are aggregated and put on the display.
  void SubmitHitTestRegionList(
      mojom::HitTestRegionListPtr hit_test_region_list);

  // Called after surfaces have been aggregated into the DisplayFrame.
  // In this call HitTestRegionList structures received from active surfaces
  // are aggregated into the HitTestRegionList structure in
  // shared memory used for event targetting.
  void Aggregate(const SurfaceId& display_surface_id);

  // Performs the work of Aggregate by creating a PostTask so that
  // the work is not directly on the call.
  void PostTaskAggregate(SurfaceId display_surface_id);

  // Called at BeginFrame. Swaps buffers in shared memory.
  void Swap();

 protected:
  // cc::SurfaceObserver:
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override {}
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override {}
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const cc::BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const cc::BeginFrameArgs& args) override {}

  // Called when a surface has been aggregated and added to the
  // display frame.  HitTestRegionList objects are held but ignored until
  // this happens.  HitTestRegionList for the surface is copied from |pending_|
  // to |active_| in this method.
  void OnSurfaceWillDraw(const SurfaceId& surface_id) override;

  // The collection of received HitTestRegionList objects that have not yet
  // been added to the DisplayFrame (OnSurfaceWillDraw has not been called).
  std::map<SurfaceId, mojom::HitTestRegionListPtr> pending_;

  // The collection of HitTestRegionList objects that have been added to the
  // DisplayFrame (OnSurfaceWillDraw has been called).
  std::map<SurfaceId, mojom::HitTestRegionListPtr> active_;

  // Keeps track of the number of regions in the active list
  // so that we know when we exceed the available length.
  int active_region_count_ = 0;

  mojo::ScopedSharedBufferHandle read_handle_;
  mojo::ScopedSharedBufferHandle write_handle_;

  // The number of elements allocated.
  int read_size_ = 0;
  int write_size_ = 0;

  mojo::ScopedSharedBufferMapping read_buffer_;
  mojo::ScopedSharedBufferMapping write_buffer_;

 private:
  // Allocates memory for the AggregatedHitTestRegion array.
  void AllocateHitTestRegionArray();
  void AllocateHitTestRegionArray(int length);

  // Appends the root element to the AggregatedHitTestRegion array.
  void AppendRoot(const SurfaceId& surface_id);

  // Appends a region to the HitTestRegionList structure to recursively
  // build the tree.
  int AppendRegion(AggregatedHitTestRegion* regions,
                   int region_index,
                   const mojom::HitTestRegionPtr& region);

  // Handles the case when this object is deleted after
  // the PostTaskAggregation call is scheduled but before invocation.
  base::WeakPtrFactory<HitTestAggregator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HitTestAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"

namespace viz {

namespace {
// TODO(gklassen): Review and select appropriate sizes based on
// telemetry / UMA.
constexpr uint32_t kMaxRegionsPerSurface = 1024;
}  // namespace

HitTestManager::HitTestManager(FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {}

HitTestManager::~HitTestManager() = default;

void HitTestManager::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    const uint64_t frame_index,
    mojom::HitTestRegionListPtr hit_test_region_list) {
  if (!ValidateHitTestRegionList(surface_id, hit_test_region_list))
    return;
  // TODO(gklassen): Runtime validation that hit_test_region_list is valid.
  // TODO(gklassen): Inform FrameSink that the hit_test_region_list is invalid.
  // TODO(gklassen): FrameSink needs to inform the host of a difficult renderer.
  hit_test_region_lists_[surface_id][frame_index] =
      std::move(hit_test_region_list);
}

bool HitTestManager::ValidateHitTestRegionList(
    const SurfaceId& surface_id,
    const mojom::HitTestRegionListPtr& hit_test_region_list) {
  if (!hit_test_region_list)
    return false;
  if (hit_test_region_list->regions.size() > kMaxRegionsPerSurface)
    return false;
  for (auto& region : hit_test_region_list->regions) {
    if (!ValidateHitTestRegion(surface_id, region))
      return false;
  }
  return true;
}

bool HitTestManager::ValidateHitTestRegion(
    const SurfaceId& surface_id,
    const mojom::HitTestRegionPtr& hit_test_region) {
  // If client_id is 0 then use the client_id that
  // matches the compositor frame.
  if (hit_test_region->frame_sink_id.client_id() == 0) {
    hit_test_region->frame_sink_id =
        FrameSinkId(surface_id.frame_sink_id().client_id(),
                    hit_test_region->frame_sink_id.sink_id());
  }
  // TODO(gklassen): Verify that this client is allowed to submit hit test
  // data for the region associated with this |frame_sink_id|.
  // TODO(gklassen): Ensure that |region->frame_sink_id| is a child of
  // |frame_sink_id|.
  if (hit_test_region->flags & mojom::kHitTestChildSurface) {
    if (!hit_test_region->local_surface_id.has_value() ||
        !hit_test_region->local_surface_id->is_valid()) {
      return false;
    }
  }
  return true;
}

bool HitTestManager::OnSurfaceDamaged(const SurfaceId& surface_id,
                                      const BeginFrameAck& ack) {
  return false;
}

void HitTestManager::OnSurfaceDiscarded(const SurfaceId& surface_id) {
  hit_test_region_lists_.erase(surface_id);
}

void HitTestManager::OnSurfaceActivated(const SurfaceId& surface_id) {
  // When a Surface is activated we can confidently remove all
  // associated HitTestRegionList objects with an older frame_index.
  auto search = hit_test_region_lists_.find(surface_id);
  if (search == hit_test_region_lists_.end())
    return;

  uint64_t frame_index = frame_sink_manager_->GetActiveFrameIndex(surface_id);

  auto& frame_index_map = search->second;
  for (auto it = frame_index_map.begin(); it != frame_index_map.end();) {
    if (it->first != frame_index)
      it = frame_index_map.erase(it);
    else
      ++it;
  }
}

const mojom::HitTestRegionList* HitTestManager::GetActiveHitTestRegionList(
    const SurfaceId& surface_id) const {
  auto search = hit_test_region_lists_.find(surface_id);
  if (search == hit_test_region_lists_.end())
    return nullptr;

  uint64_t frame_index = frame_sink_manager_->GetActiveFrameIndex(surface_id);

  auto& frame_index_map = search->second;
  auto search2 = frame_index_map.find(frame_index);
  if (search2 == frame_index_map.end())
    return nullptr;

  return search2->second.get();
}

}  // namespace viz

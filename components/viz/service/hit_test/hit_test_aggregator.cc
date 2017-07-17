// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"

namespace viz {

namespace {
// TODO(gklassen): Review and select appropriate sizes based on
// telemetry / UMA.
constexpr int kInitialSize = 1024;
constexpr int kIncrementalSize = 1024;
constexpr int kMaxRegionsPerSurface = 1024;
constexpr int kMaxSize = 100 * 1024;

bool ValidateHitTestRegion(const mojom::HitTestRegionPtr& hit_test_region) {
  if (hit_test_region->flags == mojom::kHitTestChildSurface) {
    if (!hit_test_region->surface_id.is_valid())
      return false;
  }

  return true;
}

bool ValidateHitTestRegionList(
    const mojom::HitTestRegionListPtr& hit_test_region_list) {
  if (hit_test_region_list->regions.size() > kMaxRegionsPerSurface)
    return false;
  for (auto& region : hit_test_region_list->regions) {
    if (!ValidateHitTestRegion(region))
      return false;
  }
  return true;
}

}  // namespace

HitTestAggregator::HitTestAggregator() : weak_ptr_factory_(this) {
  AllocateHitTestRegionArray();
}

HitTestAggregator::~HitTestAggregator() = default;

void HitTestAggregator::SubmitHitTestRegionList(
    mojom::HitTestRegionListPtr hit_test_region_list) {
  DCHECK(ValidateHitTestRegionList(hit_test_region_list));
  // TODO(gklassen): Runtime validation that hit_test_region_list is valid.
  // TODO(gklassen): Inform FrameSink that the hit_test_region_list is invalid.
  // TODO(gklassen): FrameSink needs to inform the host of a difficult renderer.
  pending_[hit_test_region_list->surface_id] = std::move(hit_test_region_list);
}

bool HitTestAggregator::OnSurfaceDamaged(const SurfaceId& surface_id,
                                         const cc::BeginFrameAck& ack) {
  return false;
}

void HitTestAggregator::OnSurfaceDiscarded(const SurfaceId& surface_id) {
  // Update the region count.
  auto active_search = active_.find(surface_id);
  if (active_search != active_.end()) {
    mojom::HitTestRegionList* old_hit_test_data = active_search->second.get();
    active_region_count_ -= old_hit_test_data->regions.size();
  }
  DCHECK_GE(active_region_count_, 0);

  pending_.erase(surface_id);
  active_.erase(surface_id);
}

void HitTestAggregator::OnSurfaceWillDraw(const SurfaceId& surface_id) {
  auto pending_search = pending_.find(surface_id);
  if (pending_search == pending_.end()) {
    // Have already activated pending hit_test_region_list objects for this
    // surface.
    return;
  }
  mojom::HitTestRegionList* hit_test_region_list = pending_search->second.get();

  // Update the region count.
  auto active_search = active_.find(surface_id);
  if (active_search != active_.end()) {
    mojom::HitTestRegionList* old_hit_test_data = active_search->second.get();
    active_region_count_ -= old_hit_test_data->regions.size();
  }
  active_region_count_ += hit_test_region_list->regions.size();
  DCHECK_GE(active_region_count_, 0);

  active_[surface_id] = std::move(pending_[surface_id]);
  pending_.erase(surface_id);
}

void HitTestAggregator::AllocateHitTestRegionArray() {
  AllocateHitTestRegionArray(kInitialSize);
  Swap();
  AllocateHitTestRegionArray(kInitialSize);
}

void HitTestAggregator::AllocateHitTestRegionArray(int size) {
  size_t num_bytes = size * sizeof(AggregatedHitTestRegion);
  write_handle_ = mojo::SharedBufferHandle::Create(num_bytes);
  write_size_ = size;
  write_buffer_ = write_handle_->Map(num_bytes);

  AggregatedHitTestRegion* region =
      (AggregatedHitTestRegion*)write_buffer_.get();
  region[0].child_count = kEndOfList;
}

void HitTestAggregator::PostTaskAggregate(SurfaceId display_surface_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&HitTestAggregator::Aggregate,
                     weak_ptr_factory_.GetWeakPtr(), display_surface_id));
}

void HitTestAggregator::Aggregate(const SurfaceId& display_surface_id) {
  // Check to ensure that enough memory has been allocated.
  int size = write_size_;
  int max_size = active_region_count_ + active_.size() + 1;
  if (max_size > kMaxSize)
    max_size = kMaxSize;

  if (max_size > size) {
    size = (1 + max_size / kIncrementalSize) * kIncrementalSize;
    AllocateHitTestRegionArray(size);
  }

  AppendRoot(display_surface_id);
}

void HitTestAggregator::AppendRoot(const SurfaceId& surface_id) {
  auto search = active_.find(surface_id);
  DCHECK(search != active_.end());

  mojom::HitTestRegionList* hit_test_region_list = search->second.get();

  AggregatedHitTestRegion* regions =
      static_cast<AggregatedHitTestRegion*>(write_buffer_.get());

  regions[0].frame_sink_id = hit_test_region_list->surface_id.frame_sink_id();
  regions[0].flags = hit_test_region_list->flags;
  regions[0].rect = hit_test_region_list->bounds;
  regions[0].transform = hit_test_region_list->transform;

  int region_index = 1;
  for (const auto& region : hit_test_region_list->regions) {
    if (region_index >= write_size_ - 1)
      break;
    region_index = AppendRegion(regions, region_index, region);
  }

  DCHECK_GE(region_index, 1);
  regions[0].child_count = region_index - 1;
  regions[region_index].child_count = kEndOfList;
}

int HitTestAggregator::AppendRegion(AggregatedHitTestRegion* regions,
                                    int region_index,
                                    const mojom::HitTestRegionPtr& region) {
  AggregatedHitTestRegion* element = &regions[region_index];

  element->frame_sink_id = region->surface_id.frame_sink_id();
  element->flags = region->flags;
  element->rect = region->rect;
  element->transform = region->transform;

  int parent_index = region_index++;
  if (region_index >= write_size_ - 1) {
    element->child_count = 0;
    return region_index;
  }

  if (region->flags == mojom::kHitTestChildSurface) {
    auto search = active_.find(region->surface_id);
    if (search == active_.end()) {
      // Surface HitTestRegionList not found - it may be late.
      // Don't include this region so that it doesn't receive events.
      return parent_index;
    }

    // Rather than add a node in the tree for this hit_test_region_list element
    // we can simplify the tree by merging the flags and transform into
    // the kHitTestChildSurface element.
    mojom::HitTestRegionList* hit_test_region_list = search->second.get();
    if (!hit_test_region_list->transform.IsIdentity())
      element->transform.PreconcatTransform(hit_test_region_list->transform);

    element->flags |= hit_test_region_list->flags;

    for (const auto& child_region : hit_test_region_list->regions) {
      region_index = AppendRegion(regions, region_index, child_region);
      if (region_index >= write_size_ - 1)
        break;
    }
  }
  DCHECK_GE(region_index - parent_index - 1, 0);
  element->child_count = region_index - parent_index - 1;
  return region_index;
}

void HitTestAggregator::Swap() {
  using std::swap;

  swap(read_handle_, write_handle_);
  swap(read_size_, write_size_);
  swap(read_buffer_, write_buffer_);
}

}  // namespace viz

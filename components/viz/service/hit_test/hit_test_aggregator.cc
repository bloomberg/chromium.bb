// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace viz {

namespace {
// TODO(gklassen): Review and select appropriate sizes based on
// telemetry / UMA.
constexpr uint32_t kInitialSize = 1024;
constexpr uint32_t kIncrementalSize = 1024;
constexpr uint32_t kMaxSize = 100 * 1024;

}  // namespace

HitTestAggregator::HitTestAggregator(
    const HitTestManager* hit_test_manager,
    HitTestAggregatorDelegate* delegate,
    LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate,
    const FrameSinkId& frame_sink_id)
    : hit_test_manager_(hit_test_manager),
      delegate_(delegate),
      local_surface_id_lookup_delegate_(local_surface_id_lookup_delegate),
      root_frame_sink_id_(frame_sink_id),
      weak_ptr_factory_(this) {
  AllocateHitTestRegionArray();
}

HitTestAggregator::~HitTestAggregator() = default;

void HitTestAggregator::Aggregate(const SurfaceId& display_surface_id) {
  DCHECK(referenced_child_regions_.empty());
  AppendRoot(display_surface_id);
  referenced_child_regions_.clear();
  Swap();
}

void HitTestAggregator::GrowRegionList() {
  ResizeHitTestRegionArray(write_size_ + kIncrementalSize);
}

void HitTestAggregator::Swap() {
  SwapHandles();
  if (!handle_replaced_) {
    delegate_->SwitchActiveAggregatedHitTestRegionList(root_frame_sink_id_,
                                                       active_handle_index_);
    return;
  }

  delegate_->OnAggregatedHitTestRegionListUpdated(
      root_frame_sink_id_,
      read_handle_->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY),
      read_size_,
      write_handle_->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY),
      write_size_);
  active_handle_index_ = 0;
  handle_replaced_ = false;
}

void HitTestAggregator::AllocateHitTestRegionArray() {
  ResizeHitTestRegionArray(kInitialSize);
  SwapHandles();
  ResizeHitTestRegionArray(kInitialSize);
}

void HitTestAggregator::ResizeHitTestRegionArray(uint32_t size) {
  size_t num_bytes = size * sizeof(AggregatedHitTestRegion);
  write_handle_ = mojo::SharedBufferHandle::Create(num_bytes);
  DCHECK(write_handle_.is_valid());
  auto new_buffer_ = write_handle_->Map(num_bytes);
  DCHECK(new_buffer_);
  handle_replaced_ = true;

  AggregatedHitTestRegion* region = (AggregatedHitTestRegion*)new_buffer_.get();
  if (write_size_)
    memcpy(region, write_buffer_.get(), write_size_);
  else
    region[0].child_count = kEndOfList;

  write_size_ = size;
  write_buffer_ = std::move(new_buffer_);
}

void HitTestAggregator::SwapHandles() {
  using std::swap;

  swap(read_handle_, write_handle_);
  swap(read_size_, write_size_);
  swap(read_buffer_, write_buffer_);
  active_handle_index_ = !active_handle_index_;
}

void HitTestAggregator::AppendRoot(const SurfaceId& surface_id) {
  SCOPED_UMA_HISTOGRAM_TIMER("Event.VizHitTest.AggregateTime");

  const mojom::HitTestRegionList* hit_test_region_list =
      hit_test_manager_->GetActiveHitTestRegionList(
          local_surface_id_lookup_delegate_, surface_id.frame_sink_id());
  if (!hit_test_region_list)
    return;

  referenced_child_regions_.insert(surface_id.frame_sink_id());

  size_t region_index = 1;
  for (const auto& region : hit_test_region_list->regions) {
    if (region_index >= write_size_ - 1)
      break;
    region_index = AppendRegion(region_index, region);
  }

  DCHECK_GE(region_index, 1u);
  int32_t child_count = region_index - 1;
  UMA_HISTOGRAM_COUNTS_1000("Event.VizHitTest.HitTestRegions", region_index);
  SetRegionAt(0, surface_id.frame_sink_id(), hit_test_region_list->flags,
              hit_test_region_list->bounds, hit_test_region_list->transform,
              child_count);
  MarkEndAt(region_index);
}

size_t HitTestAggregator::AppendRegion(size_t region_index,
                                       const mojom::HitTestRegionPtr& region) {
  size_t parent_index = region_index++;
  if (region_index >= write_size_ - 1) {
    if (write_size_ > kMaxSize) {
      MarkEndAt(parent_index);
      return region_index;
    } else {
      GrowRegionList();
    }
  }

  uint32_t flags = region->flags;
  gfx::Transform transform = region->transform;

  if (region->flags & mojom::kHitTestChildSurface) {
    if (referenced_child_regions_.count(region->frame_sink_id))
      return parent_index;

    const mojom::HitTestRegionList* hit_test_region_list =
        hit_test_manager_->GetActiveHitTestRegionList(
            local_surface_id_lookup_delegate_, region->frame_sink_id);
    if (!hit_test_region_list) {
      // Hit-test data not found with this FrameSinkId. This means that it
      // failed to find a surface corresponding to this FrameSinkId at surface
      // aggregation time.
      return parent_index;
    }

    referenced_child_regions_.insert(region->frame_sink_id);

    // Rather than add a node in the tree for this hit_test_region_list
    // element we can simplify the tree by merging the flags and transform
    // into the kHitTestChildSurface element.
    if (!hit_test_region_list->transform.IsIdentity())
      transform.PreconcatTransform(hit_test_region_list->transform);

    flags |= hit_test_region_list->flags;

    for (const auto& child_region : hit_test_region_list->regions) {
      region_index = AppendRegion(region_index, child_region);
      if (region_index >= write_size_ - 1)
        break;
    }
  }
  DCHECK_GE(region_index - parent_index - 1, 0u);
  int32_t child_count = region_index - parent_index - 1;
  SetRegionAt(parent_index, region->frame_sink_id, flags, region->rect,
              transform, child_count);
  return region_index;
}

void HitTestAggregator::SetRegionAt(size_t index,
                                    const FrameSinkId& frame_sink_id,
                                    uint32_t flags,
                                    const gfx::Rect& rect,
                                    const gfx::Transform& transform,
                                    int32_t child_count) {
  AggregatedHitTestRegion* regions =
      static_cast<AggregatedHitTestRegion*>(write_buffer_.get());
  AggregatedHitTestRegion* element = &regions[index];

  element->frame_sink_id = frame_sink_id;
  element->flags = flags;
  element->rect = rect;
  element->child_count = child_count;
  element->set_transform(transform);
}

void HitTestAggregator::MarkEndAt(size_t index) {
  AggregatedHitTestRegion* regions =
      static_cast<AggregatedHitTestRegion*>(write_buffer_.get());
  regions[index].child_count = kEndOfList;
}

}  // namespace viz

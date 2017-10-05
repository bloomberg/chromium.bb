// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

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

void PrepareTransformForReadOnlySharedMemory(gfx::Transform* transform) {
  // |transform| is going to be shared in read-only memory to HitTestQuery.
  // However, if HitTestQuery tries to operate on it, then it is possible that
  // it will attempt to perform write on the underlying SkMatrix44 [1], causing
  // invalid memory write in read-only memory.
  // [1]
  // https://cs.chromium.org/chromium/src/third_party/skia/include/core/SkMatrix44.h?l=133
  // Explicitly calling getType() to compute the type-mask in SkMatrix44.
  transform->matrix().getType();
}

}  // namespace

HitTestAggregator::HitTestAggregator(const HitTestManager* hit_test_manager,
                                     HitTestAggregatorDelegate* delegate)
    : hit_test_manager_(hit_test_manager),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  AllocateHitTestRegionArray();
}

HitTestAggregator::~HitTestAggregator() = default;

void HitTestAggregator::PostTaskAggregate(const SurfaceId& display_surface_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&HitTestAggregator::Aggregate,
                     weak_ptr_factory_.GetWeakPtr(), display_surface_id));
}

void HitTestAggregator::Aggregate(const SurfaceId& display_surface_id) {
  AppendRoot(display_surface_id);
  Swap();
}

void HitTestAggregator::GrowRegionList() {
  ResizeHitTestRegionArray(write_size_ + kIncrementalSize);
}

void HitTestAggregator::Swap() {
  SwapHandles();
  if (!handle_replaced_) {
    delegate_->SwitchActiveAggregatedHitTestRegionList(active_handle_index_);
    return;
  }

  delegate_->OnAggregatedHitTestRegionListUpdated(
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
  auto new_buffer_ = write_handle_->Map(num_bytes);
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
  const mojom::HitTestRegionList* hit_test_region_list =
      hit_test_manager_->GetActiveHitTestRegionList(surface_id);
  if (!hit_test_region_list)
    return;

  size_t region_index = 1;
  for (const auto& region : hit_test_region_list->regions) {
    if (region_index >= write_size_ - 1)
      break;
    region_index = AppendRegion(region_index, region);
  }

  DCHECK_GE(region_index, 1u);
  int32_t child_count = region_index - 1;
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
    auto surface_id =
        SurfaceId(region->frame_sink_id, region->local_surface_id.value());
    const mojom::HitTestRegionList* hit_test_region_list =
        hit_test_manager_->GetActiveHitTestRegionList(surface_id);
    if (!hit_test_region_list) {
      // Surface HitTestRegionList not found - it may be late.
      // Don't include this region so that it doesn't receive events.
      return parent_index;
    }

    // Rather than add a node in the tree for this hit_test_region_list element
    // we can simplify the tree by merging the flags and transform into
    // the kHitTestChildSurface element.
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
  element->transform = transform;
  element->child_count = child_count;

  PrepareTransformForReadOnlySharedMemory(&element->transform);
}

void HitTestAggregator::MarkEndAt(size_t index) {
  AggregatedHitTestRegion* regions =
      static_cast<AggregatedHitTestRegion*>(write_buffer_.get());
  regions[index].child_count = kEndOfList;
}

}  // namespace viz

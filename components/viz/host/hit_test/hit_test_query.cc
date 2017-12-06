// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/hit_test/hit_test_query.h"

#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"

namespace viz {
namespace {

// If we want to add new source type here, consider switching to use
// ui::EventPointerType instead of EventSource.
bool ShouldUseTouchBounds(EventSource event_source) {
  return event_source == EventSource::TOUCH;
}

}  // namespace

HitTestQuery::HitTestQuery() = default;

HitTestQuery::~HitTestQuery() = default;

void HitTestQuery::OnAggregatedHitTestRegionListUpdated(
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  DCHECK(active_handle.is_valid() && idle_handle.is_valid());
  handle_buffer_sizes_[0] = active_handle_size;
  handle_buffers_[0] = active_handle->Map(handle_buffer_sizes_[0] *
                                          sizeof(AggregatedHitTestRegion));
  handle_buffer_sizes_[1] = idle_handle_size;
  handle_buffers_[1] = idle_handle->Map(handle_buffer_sizes_[1] *
                                        sizeof(AggregatedHitTestRegion));
  if (!handle_buffers_[0] || !handle_buffers_[1]) {
    // TODO(riajiang): Report security fault. http://crbug.com/746470
    NOTREACHED();
    return;
  }
  SwitchActiveAggregatedHitTestRegionList(0);
}

void HitTestQuery::SwitchActiveAggregatedHitTestRegionList(
    uint8_t active_handle_index) {
  DCHECK(active_handle_index == 0u || active_handle_index == 1u);
  active_hit_test_list_ = static_cast<AggregatedHitTestRegion*>(
      handle_buffers_[active_handle_index].get());
  active_hit_test_list_size_ = handle_buffer_sizes_[active_handle_index];
}

Target HitTestQuery::FindTargetForLocation(
    EventSource event_source,
    const gfx::Point& location_in_root) const {
  Target target;
  if (!active_hit_test_list_size_)
    return target;

  FindTargetInRegionForLocation(event_source, location_in_root,
                                active_hit_test_list_, &target);
  return target;
}

gfx::Point HitTestQuery::TransformLocationForTarget(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    const gfx::Point& location_in_root) const {
  if (!active_hit_test_list_size_)
    return location_in_root;

  gfx::Point location_in_target(location_in_root);
  // TODO(riajiang): Cache the matrix product such that the transform can be
  // done immediately. crbug/758062.
  DCHECK(target_ancestors.size() > 0u &&
         target_ancestors[target_ancestors.size() - 1] ==
             active_hit_test_list_->frame_sink_id);
  bool success = TransformLocationForTargetRecursively(
      event_source, target_ancestors, target_ancestors.size() - 1,
      active_hit_test_list_, &location_in_target);
  // Must provide a valid target.
  DCHECK(success);
  return location_in_target;
}

bool HitTestQuery::FindTargetInRegionForLocation(
    EventSource event_source,
    const gfx::Point& location_in_parent,
    AggregatedHitTestRegion* region,
    Target* target) const {
  gfx::Point location_transformed(location_in_parent);
  region->transform.TransformPoint(&location_transformed);
  if (!region->rect.Contains(location_transformed))
    return false;

  if (region->child_count < 0 ||
      region->child_count >
          (active_hit_test_list_ + active_hit_test_list_size_ - region - 1)) {
    return false;
  }
  AggregatedHitTestRegion* child_region = region + 1;
  AggregatedHitTestRegion* child_region_end =
      child_region + region->child_count;
  gfx::Point location_in_target(location_transformed);
  location_in_target.Offset(-region->rect.x(), -region->rect.y());
  while (child_region < child_region_end) {
    if (FindTargetInRegionForLocation(event_source, location_in_target,
                                      child_region, target)) {
      return true;
    }

    if (child_region->child_count < 0 ||
        child_region->child_count >= region->child_count) {
      return false;
    }
    child_region = child_region + child_region->child_count + 1;
  }

  bool match_touch_or_mouse_region =
      ShouldUseTouchBounds(event_source)
          ? (region->flags & mojom::kHitTestTouch) != 0u
          : (region->flags & mojom::kHitTestMouse) != 0u;
  if (!match_touch_or_mouse_region)
    return false;
  if (region->flags & mojom::kHitTestMine) {
    target->frame_sink_id = region->frame_sink_id;
    target->location_in_target = location_in_target;
    target->flags = region->flags;
    return true;
  }
  if (region->flags & mojom::kHitTestAsk) {
    target->flags = region->flags;
    return true;
  }
  return false;
}

bool HitTestQuery::TransformLocationForTargetRecursively(
    EventSource event_source,
    const std::vector<FrameSinkId>& target_ancestors,
    size_t target_ancestor,
    AggregatedHitTestRegion* region,
    gfx::Point* location_in_target) const {
  bool match_touch_or_mouse_region =
      ShouldUseTouchBounds(event_source)
          ? (region->flags & mojom::kHitTestTouch) != 0u
          : (region->flags & mojom::kHitTestMouse) != 0u;
  if ((region->flags & mojom::kHitTestChildSurface) == 0u &&
      !match_touch_or_mouse_region) {
    return false;
  }

  region->transform.TransformPoint(location_in_target);
  location_in_target->Offset(-region->rect.x(), -region->rect.y());
  if (!target_ancestor)
    return true;

  if (region->child_count < 0 ||
      region->child_count >
          (active_hit_test_list_ + active_hit_test_list_size_ - region - 1)) {
    return false;
  }
  AggregatedHitTestRegion* child_region = region + 1;
  AggregatedHitTestRegion* child_region_end =
      child_region + region->child_count;
  while (child_region < child_region_end) {
    if (child_region->frame_sink_id == target_ancestors[target_ancestor - 1]) {
      return TransformLocationForTargetRecursively(
          event_source, target_ancestors, target_ancestor - 1, child_region,
          location_in_target);
    }

    if (child_region->child_count < 0 ||
        child_region->child_count >= region->child_count) {
      return false;
    }
    child_region = child_region + child_region->child_count + 1;
  }

  return false;
}

}  // namespace viz

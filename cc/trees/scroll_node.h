// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_NODE_H_
#define CC_TREES_SCROLL_NODE_H_

#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

namespace proto {
class TreeNode;
}  // namespace proto

struct CC_EXPORT ScrollNode {
  ScrollNode();
  ScrollNode(const ScrollNode& other);

  int id;
  int parent_id;
  int owner_id;

  bool scrollable;
  uint32_t main_thread_scrolling_reasons;
  bool contains_non_fast_scrollable_region;
  gfx::Size scroll_clip_layer_bounds;
  gfx::Size bounds;
  bool max_scroll_offset_affected_by_page_scale;
  bool is_inner_viewport_scroll_layer;
  bool is_outer_viewport_scroll_layer;
  gfx::Vector2dF offset_to_transform_parent;
  bool should_flatten;
  bool user_scrollable_horizontal;
  bool user_scrollable_vertical;
  ElementId element_id;
  int transform_id;
  // Number of drawn layers pointing to this node or any of its descendants.
  int num_drawn_descendants;

  bool operator==(const ScrollNode& other) const;

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_SCROLL_NODE_H_

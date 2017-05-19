// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer.h"
#include "cc/trees/element_id.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"

namespace cc {

ScrollNode::ScrollNode()
    : id(ScrollTree::kInvalidNodeId),
      parent_id(ScrollTree::kInvalidNodeId),
      owning_layer_id(Layer::INVALID_ID),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      scrollable(false),
      max_scroll_offset_affected_by_page_scale(false),
      scrolls_inner_viewport(false),
      scrolls_outer_viewport(false),
      should_flatten(false),
      user_scrollable_horizontal(false),
      user_scrollable_vertical(false),
      transform_id(0) {}

ScrollNode::ScrollNode(const ScrollNode& other) = default;

bool ScrollNode::operator==(const ScrollNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owning_layer_id == other.owning_layer_id &&
         scrollable == other.scrollable &&
         main_thread_scrolling_reasons == other.main_thread_scrolling_reasons &&
         non_fast_scrollable_region == other.non_fast_scrollable_region &&
         scroll_clip_layer_bounds == other.scroll_clip_layer_bounds &&
         bounds == other.bounds &&
         max_scroll_offset_affected_by_page_scale ==
             other.max_scroll_offset_affected_by_page_scale &&
         scrolls_inner_viewport == other.scrolls_inner_viewport &&
         scrolls_outer_viewport == other.scrolls_outer_viewport &&
         offset_to_transform_parent == other.offset_to_transform_parent &&
         should_flatten == other.should_flatten &&
         user_scrollable_horizontal == other.user_scrollable_horizontal &&
         user_scrollable_vertical == other.user_scrollable_vertical &&
         element_id == other.element_id && transform_id == other.transform_id;
}

void ScrollNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owning_layer_id", owning_layer_id);
  value->SetBoolean("scrollable", scrollable);
  MathUtil::AddToTracedValue("scroll_clip_layer_bounds",
                             scroll_clip_layer_bounds, value);
  MathUtil::AddToTracedValue("bounds", bounds, value);
  MathUtil::AddToTracedValue("offset_to_transform_parent",
                             offset_to_transform_parent, value);
  value->SetBoolean("should_flatten", should_flatten);
  value->SetBoolean("user_scrollable_horizontal", user_scrollable_horizontal);
  value->SetBoolean("user_scrollable_vertical", user_scrollable_vertical);

  element_id.AddToTracedValue(value);
  value->SetInteger("transform_id", transform_id);
}

}  // namespace cc

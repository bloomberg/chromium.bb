// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/element_id.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/trees/scroll_node.h"

namespace cc {

ScrollNode::ScrollNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      scrollable(false),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      contains_non_fast_scrollable_region(false),
      max_scroll_offset_affected_by_page_scale(false),
      is_inner_viewport_scroll_layer(false),
      is_outer_viewport_scroll_layer(false),
      should_flatten(false),
      user_scrollable_horizontal(false),
      user_scrollable_vertical(false),
      transform_id(0),
      num_drawn_descendants(0) {}

ScrollNode::ScrollNode(const ScrollNode& other) = default;

bool ScrollNode::operator==(const ScrollNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && scrollable == other.scrollable &&
         main_thread_scrolling_reasons == other.main_thread_scrolling_reasons &&
         contains_non_fast_scrollable_region ==
             other.contains_non_fast_scrollable_region &&
         scroll_clip_layer_bounds == other.scroll_clip_layer_bounds &&
         bounds == other.bounds &&
         max_scroll_offset_affected_by_page_scale ==
             other.max_scroll_offset_affected_by_page_scale &&
         is_inner_viewport_scroll_layer ==
             other.is_inner_viewport_scroll_layer &&
         is_outer_viewport_scroll_layer ==
             other.is_outer_viewport_scroll_layer &&
         offset_to_transform_parent == other.offset_to_transform_parent &&
         should_flatten == other.should_flatten &&
         user_scrollable_horizontal == other.user_scrollable_horizontal &&
         user_scrollable_vertical == other.user_scrollable_vertical &&
         element_id == other.element_id && transform_id == other.transform_id;
}

void ScrollNode::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);

  DCHECK(!proto->has_scroll_node_data());
  proto::ScrollNodeData* data = proto->mutable_scroll_node_data();
  data->set_scrollable(scrollable);
  data->set_main_thread_scrolling_reasons(main_thread_scrolling_reasons);
  data->set_contains_non_fast_scrollable_region(
      contains_non_fast_scrollable_region);
  SizeToProto(scroll_clip_layer_bounds,
              data->mutable_scroll_clip_layer_bounds());
  SizeToProto(bounds, data->mutable_bounds());
  data->set_max_scroll_offset_affected_by_page_scale(
      max_scroll_offset_affected_by_page_scale);
  data->set_is_inner_viewport_scroll_layer(is_inner_viewport_scroll_layer);
  data->set_is_outer_viewport_scroll_layer(is_outer_viewport_scroll_layer);
  Vector2dFToProto(offset_to_transform_parent,
                   data->mutable_offset_to_transform_parent());
  data->set_should_flatten(should_flatten);
  data->set_user_scrollable_horizontal(user_scrollable_horizontal);
  data->set_user_scrollable_vertical(user_scrollable_vertical);
  element_id.ToProtobuf(data->mutable_element_id());
  data->set_transform_id(transform_id);
}

void ScrollNode::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();

  DCHECK(proto.has_scroll_node_data());
  const proto::ScrollNodeData& data = proto.scroll_node_data();

  scrollable = data.scrollable();
  main_thread_scrolling_reasons = data.main_thread_scrolling_reasons();
  contains_non_fast_scrollable_region =
      data.contains_non_fast_scrollable_region();
  scroll_clip_layer_bounds = ProtoToSize(data.scroll_clip_layer_bounds());
  bounds = ProtoToSize(data.bounds());
  max_scroll_offset_affected_by_page_scale =
      data.max_scroll_offset_affected_by_page_scale();
  is_inner_viewport_scroll_layer = data.is_inner_viewport_scroll_layer();
  is_outer_viewport_scroll_layer = data.is_outer_viewport_scroll_layer();
  offset_to_transform_parent =
      ProtoToVector2dF(data.offset_to_transform_parent());
  should_flatten = data.should_flatten();
  user_scrollable_horizontal = data.user_scrollable_horizontal();
  user_scrollable_vertical = data.user_scrollable_vertical();
  element_id.FromProtobuf(data.element_id());
  transform_id = data.transform_id();
}

void ScrollNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
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

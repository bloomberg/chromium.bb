// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

TransformNode::TransformNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      sticky_position_constraint_id(-1),
      source_node_id(-1),
      sorting_context_id(0),
      needs_local_transform_update(true),
      node_and_ancestors_are_animated_or_invertible(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      has_potential_animation(false),
      is_currently_animating(false),
      to_screen_is_potentially_animated(false),
      has_only_translation_animations(true),
      flattens_inherited_transform(false),
      node_and_ancestors_are_flat(true),
      node_and_ancestors_have_only_integer_translation(true),
      scrolls(false),
      should_be_snapped(false),
      moved_by_inner_viewport_bounds_delta_x(false),
      moved_by_inner_viewport_bounds_delta_y(false),
      moved_by_outer_viewport_bounds_delta_x(false),
      moved_by_outer_viewport_bounds_delta_y(false),
      in_subtree_of_page_scale_layer(false),
      transform_changed(false),
      post_local_scale_factor(1.0f) {}

TransformNode::TransformNode(const TransformNode&) = default;

bool TransformNode::operator==(const TransformNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && pre_local == other.pre_local &&
         local == other.local && post_local == other.post_local &&
         to_parent == other.to_parent &&
         source_node_id == other.source_node_id &&
         sorting_context_id == other.sorting_context_id &&
         needs_local_transform_update == other.needs_local_transform_update &&
         node_and_ancestors_are_animated_or_invertible ==
             other.node_and_ancestors_are_animated_or_invertible &&
         is_invertible == other.is_invertible &&
         ancestors_are_invertible == other.ancestors_are_invertible &&
         has_potential_animation == other.has_potential_animation &&
         is_currently_animating == other.is_currently_animating &&
         to_screen_is_potentially_animated ==
             other.to_screen_is_potentially_animated &&
         has_only_translation_animations ==
             other.has_only_translation_animations &&
         flattens_inherited_transform == other.flattens_inherited_transform &&
         node_and_ancestors_are_flat == other.node_and_ancestors_are_flat &&
         node_and_ancestors_have_only_integer_translation ==
             other.node_and_ancestors_have_only_integer_translation &&
         scrolls == other.scrolls &&
         should_be_snapped == other.should_be_snapped &&
         moved_by_inner_viewport_bounds_delta_x ==
             other.moved_by_inner_viewport_bounds_delta_x &&
         moved_by_inner_viewport_bounds_delta_y ==
             other.moved_by_inner_viewport_bounds_delta_y &&
         moved_by_outer_viewport_bounds_delta_x ==
             other.moved_by_outer_viewport_bounds_delta_x &&
         moved_by_outer_viewport_bounds_delta_y ==
             other.moved_by_outer_viewport_bounds_delta_y &&
         in_subtree_of_page_scale_layer ==
             other.in_subtree_of_page_scale_layer &&
         transform_changed == other.transform_changed &&
         post_local_scale_factor == other.post_local_scale_factor &&
         scroll_offset == other.scroll_offset &&
         snap_amount == other.snap_amount &&
         source_offset == other.source_offset &&
         source_to_parent == other.source_to_parent;
}

void TransformNode::update_pre_local_transform(
    const gfx::Point3F& transform_origin) {
  pre_local.MakeIdentity();
  pre_local.Translate3d(-transform_origin.x(), -transform_origin.y(),
                        -transform_origin.z());
}

void TransformNode::update_post_local_transform(
    const gfx::PointF& position,
    const gfx::Point3F& transform_origin) {
  post_local.MakeIdentity();
  post_local.Scale(post_local_scale_factor, post_local_scale_factor);
  post_local.Translate3d(
      position.x() + source_offset.x() + transform_origin.x(),
      position.y() + source_offset.y() + transform_origin.y(),
      transform_origin.z());
}

void TransformNode::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);

  DCHECK(!proto->has_transform_node_data());
  proto::TranformNodeData* data = proto->mutable_transform_node_data();

  TransformToProto(pre_local, data->mutable_pre_local());
  TransformToProto(local, data->mutable_local());
  TransformToProto(post_local, data->mutable_post_local());

  TransformToProto(to_parent, data->mutable_to_parent());

  data->set_source_node_id(source_node_id);
  data->set_sorting_context_id(sorting_context_id);

  data->set_needs_local_transform_update(needs_local_transform_update);

  data->set_node_and_ancestors_are_animated_or_invertible(
      node_and_ancestors_are_animated_or_invertible);

  data->set_is_invertible(is_invertible);
  data->set_ancestors_are_invertible(ancestors_are_invertible);

  data->set_has_potential_animation(has_potential_animation);
  data->set_is_currently_animating(is_currently_animating);
  data->set_to_screen_is_potentially_animated(
      to_screen_is_potentially_animated);
  data->set_has_only_translation_animations(has_only_translation_animations);

  data->set_flattens_inherited_transform(flattens_inherited_transform);
  data->set_node_and_ancestors_are_flat(node_and_ancestors_are_flat);

  data->set_node_and_ancestors_have_only_integer_translation(
      node_and_ancestors_have_only_integer_translation);
  data->set_scrolls(scrolls);
  data->set_should_be_snapped(should_be_snapped);

  data->set_moved_by_inner_viewport_bounds_delta_x(
      moved_by_inner_viewport_bounds_delta_x);
  data->set_moved_by_inner_viewport_bounds_delta_y(
      moved_by_inner_viewport_bounds_delta_y);
  data->set_moved_by_outer_viewport_bounds_delta_x(
      moved_by_outer_viewport_bounds_delta_x);
  data->set_moved_by_outer_viewport_bounds_delta_y(
      moved_by_outer_viewport_bounds_delta_y);

  data->set_in_subtree_of_page_scale_layer(in_subtree_of_page_scale_layer);
  data->set_transform_changed(transform_changed);
  data->set_post_local_scale_factor(post_local_scale_factor);

  ScrollOffsetToProto(scroll_offset, data->mutable_scroll_offset());
  Vector2dFToProto(snap_amount, data->mutable_snap_amount());
  Vector2dFToProto(source_offset, data->mutable_source_offset());
  Vector2dFToProto(source_to_parent, data->mutable_source_to_parent());
}

void TransformNode::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();

  DCHECK(proto.has_transform_node_data());
  const proto::TranformNodeData& data = proto.transform_node_data();

  pre_local = ProtoToTransform(data.pre_local());
  local = ProtoToTransform(data.local());
  post_local = ProtoToTransform(data.post_local());

  to_parent = ProtoToTransform(data.to_parent());

  source_node_id = data.source_node_id();
  sorting_context_id = data.sorting_context_id();

  needs_local_transform_update = data.needs_local_transform_update();

  node_and_ancestors_are_animated_or_invertible =
      data.node_and_ancestors_are_animated_or_invertible();

  is_invertible = data.is_invertible();
  ancestors_are_invertible = data.ancestors_are_invertible();

  has_potential_animation = data.has_potential_animation();
  is_currently_animating = data.is_currently_animating();
  to_screen_is_potentially_animated = data.to_screen_is_potentially_animated();
  has_only_translation_animations = data.has_only_translation_animations();

  flattens_inherited_transform = data.flattens_inherited_transform();
  node_and_ancestors_are_flat = data.node_and_ancestors_are_flat();

  node_and_ancestors_have_only_integer_translation =
      data.node_and_ancestors_have_only_integer_translation();
  scrolls = data.scrolls();
  should_be_snapped = data.should_be_snapped();

  moved_by_inner_viewport_bounds_delta_x =
      data.moved_by_inner_viewport_bounds_delta_x();
  moved_by_inner_viewport_bounds_delta_y =
      data.moved_by_inner_viewport_bounds_delta_y();
  moved_by_outer_viewport_bounds_delta_x =
      data.moved_by_outer_viewport_bounds_delta_x();
  moved_by_outer_viewport_bounds_delta_y =
      data.moved_by_outer_viewport_bounds_delta_y();

  in_subtree_of_page_scale_layer = data.in_subtree_of_page_scale_layer();
  transform_changed = data.transform_changed();
  post_local_scale_factor = data.post_local_scale_factor();

  scroll_offset = ProtoToScrollOffset(data.scroll_offset());
  snap_amount = ProtoToVector2dF(data.snap_amount());
  source_offset = ProtoToVector2dF(data.source_offset());
  source_to_parent = ProtoToVector2dF(data.source_to_parent());
}

void TransformNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  MathUtil::AddToTracedValue("pre_local", pre_local, value);
  MathUtil::AddToTracedValue("local", local, value);
  MathUtil::AddToTracedValue("post_local", post_local, value);
  // TODO(sunxd): make frameviewer work without target_id
  value->SetInteger("target_id", 0);
  value->SetInteger("content_target_id", 0);
  value->SetInteger("source_node_id", source_node_id);
  value->SetInteger("sorting_context_id", sorting_context_id);
  MathUtil::AddToTracedValue("scroll_offset", scroll_offset, value);
  MathUtil::AddToTracedValue("snap_amount", snap_amount, value);
}

TransformCachedNodeData::TransformCachedNodeData()
    : target_id(-1), content_target_id(-1) {}

TransformCachedNodeData::TransformCachedNodeData(
    const TransformCachedNodeData& other) = default;

TransformCachedNodeData::~TransformCachedNodeData() {}

bool TransformCachedNodeData::operator==(
    const TransformCachedNodeData& other) const {
  return from_screen == other.from_screen && to_screen == other.to_screen &&
         target_id == other.target_id &&
         content_target_id == other.content_target_id;
}

void TransformCachedNodeData::ToProtobuf(
    proto::TransformCachedNodeData* proto) const {
  TransformToProto(from_screen, proto->mutable_from_screen());
  TransformToProto(to_screen, proto->mutable_to_screen());
  proto->set_target_id(target_id);
  proto->set_content_target_id(content_target_id);
}

void TransformCachedNodeData::FromProtobuf(
    const proto::TransformCachedNodeData& proto) {
  from_screen = ProtoToTransform(proto.from_screen());
  to_screen = ProtoToTransform(proto.to_screen());
  target_id = proto.target_id();
  content_target_id = proto.content_target_id();
}

}  // namespace cc

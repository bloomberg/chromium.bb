// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/logging.h"
#include "cc/base/math_util.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/proto/scroll_offset.pb.h"
#include "cc/proto/transform.pb.h"
#include "cc/proto/vector2df.pb.h"
#include "cc/trees/property_tree.h"

namespace cc {

template <typename T>
bool TreeNode<T>::operator==(const TreeNode<T>& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && data == other.data;
}

template <typename T>
void TreeNode<T>::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);
  data.ToProtobuf(proto);
}

template <typename T>
void TreeNode<T>::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();
  data.FromProtobuf(proto);
}

template struct TreeNode<TransformNodeData>;
template struct TreeNode<ClipNodeData>;
template struct TreeNode<EffectNodeData>;
template struct TreeNode<ScrollNodeData>;

template <typename T>
PropertyTree<T>::PropertyTree()
    : needs_update_(false) {
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
PropertyTree<T>::~PropertyTree() {
}

TransformTree::TransformTree()
    : source_to_parent_updates_allowed_(true),
      page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      device_transform_scale_factor_(1.f) {}

TransformTree::~TransformTree() {
}

template <typename T>
int PropertyTree<T>::Insert(const T& tree_node, int parent_id) {
  DCHECK_GT(nodes_.size(), 0u);
  nodes_.push_back(tree_node);
  T& node = nodes_.back();
  node.parent_id = parent_id;
  node.id = static_cast<int>(nodes_.size()) - 1;
  return node.id;
}

template <typename T>
void PropertyTree<T>::clear() {
  nodes_.clear();
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
bool PropertyTree<T>::operator==(const PropertyTree<T>& other) const {
  return nodes_ == other.nodes() && needs_update_ == other.needs_update();
}

template <typename T>
void PropertyTree<T>::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK_EQ(0, proto->nodes_size());
  for (const auto& node : nodes_)
    node.ToProtobuf(proto->add_nodes());
  proto->set_needs_update(needs_update_);
}

template <typename T>
void PropertyTree<T>::FromProtobuf(const proto::PropertyTree& proto) {
  // Verify that the property tree is empty.
  DCHECK_EQ(static_cast<int>(nodes_.size()), 1);
  DCHECK_EQ(back()->id, 0);
  DCHECK_EQ(back()->parent_id, -1);

  // Add the first node.
  DCHECK_GT(proto.nodes_size(), 0);
  nodes_.back().FromProtobuf(proto.nodes(0));

  for (int i = 1; i < proto.nodes_size(); ++i) {
    nodes_.push_back(T());
    nodes_.back().FromProtobuf(proto.nodes(i));
  }

  needs_update_ = proto.needs_update();
}

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<EffectNode>;
template class PropertyTree<ScrollNode>;

TransformNodeData::TransformNodeData()
    : target_id(-1),
      content_target_id(-1),
      source_node_id(-1),
      sorting_context_id(0),
      needs_local_transform_update(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      is_animated(false),
      to_screen_is_animated(false),
      has_only_translation_animations(true),
      to_screen_has_scale_animation(false),
      flattens_inherited_transform(false),
      node_and_ancestors_are_flat(true),
      node_and_ancestors_have_only_integer_translation(true),
      scrolls(false),
      needs_sublayer_scale(false),
      affected_by_inner_viewport_bounds_delta_x(false),
      affected_by_inner_viewport_bounds_delta_y(false),
      affected_by_outer_viewport_bounds_delta_x(false),
      affected_by_outer_viewport_bounds_delta_y(false),
      in_subtree_of_page_scale_layer(false),
      post_local_scale_factor(1.0f),
      local_maximum_animation_target_scale(0.f),
      local_starting_animation_scale(0.f),
      combined_maximum_animation_target_scale(0.f),
      combined_starting_animation_scale(0.f) {}

TransformNodeData::~TransformNodeData() {
}

bool TransformNodeData::operator==(const TransformNodeData& other) const {
  return pre_local == other.pre_local && local == other.local &&
         post_local == other.post_local && to_parent == other.to_parent &&
         to_target == other.to_target && from_target == other.from_target &&
         to_screen == other.to_screen && from_screen == other.from_screen &&
         target_id == other.target_id &&
         content_target_id == other.content_target_id &&
         source_node_id == other.source_node_id &&
         sorting_context_id == other.sorting_context_id &&
         needs_local_transform_update == other.needs_local_transform_update &&
         is_invertible == other.is_invertible &&
         ancestors_are_invertible == other.ancestors_are_invertible &&
         is_animated == other.is_animated &&
         to_screen_is_animated == other.to_screen_is_animated &&
         has_only_translation_animations ==
             other.has_only_translation_animations &&
         to_screen_has_scale_animation == other.to_screen_has_scale_animation &&
         flattens_inherited_transform == other.flattens_inherited_transform &&
         node_and_ancestors_are_flat == other.node_and_ancestors_are_flat &&
         node_and_ancestors_have_only_integer_translation ==
             other.node_and_ancestors_have_only_integer_translation &&
         scrolls == other.scrolls &&
         needs_sublayer_scale == other.needs_sublayer_scale &&
         affected_by_inner_viewport_bounds_delta_x ==
             other.affected_by_inner_viewport_bounds_delta_x &&
         affected_by_inner_viewport_bounds_delta_y ==
             other.affected_by_inner_viewport_bounds_delta_y &&
         affected_by_outer_viewport_bounds_delta_x ==
             other.affected_by_outer_viewport_bounds_delta_x &&
         affected_by_outer_viewport_bounds_delta_y ==
             other.affected_by_outer_viewport_bounds_delta_y &&
         in_subtree_of_page_scale_layer ==
             other.in_subtree_of_page_scale_layer &&
         post_local_scale_factor == other.post_local_scale_factor &&
         local_maximum_animation_target_scale ==
             other.local_maximum_animation_target_scale &&
         local_starting_animation_scale ==
             other.local_starting_animation_scale &&
         combined_maximum_animation_target_scale ==
             other.combined_maximum_animation_target_scale &&
         combined_starting_animation_scale ==
             other.combined_starting_animation_scale &&
         sublayer_scale == other.sublayer_scale &&
         scroll_offset == other.scroll_offset &&
         scroll_snap == other.scroll_snap &&
         source_offset == other.source_offset &&
         source_to_parent == other.source_to_parent;
}

void TransformNodeData::update_pre_local_transform(
    const gfx::Point3F& transform_origin) {
  pre_local.MakeIdentity();
  pre_local.Translate3d(-transform_origin.x(), -transform_origin.y(),
                        -transform_origin.z());
}

void TransformNodeData::update_post_local_transform(
    const gfx::PointF& position,
    const gfx::Point3F& transform_origin) {
  post_local.MakeIdentity();
  post_local.Scale(post_local_scale_factor, post_local_scale_factor);
  post_local.Translate3d(
      position.x() + source_offset.x() + transform_origin.x(),
      position.y() + source_offset.y() + transform_origin.y(),
      transform_origin.z());
}

void TransformNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_transform_node_data());
  proto::TranformNodeData* data = proto->mutable_transform_node_data();

  TransformToProto(pre_local, data->mutable_pre_local());
  TransformToProto(local, data->mutable_local());
  TransformToProto(post_local, data->mutable_post_local());

  TransformToProto(to_parent, data->mutable_to_parent());

  TransformToProto(to_target, data->mutable_to_target());
  TransformToProto(from_target, data->mutable_from_target());

  TransformToProto(to_screen, data->mutable_to_screen());
  TransformToProto(from_screen, data->mutable_from_screen());

  data->set_target_id(target_id);
  data->set_content_target_id(content_target_id);
  data->set_source_node_id(source_node_id);
  data->set_sorting_context_id(sorting_context_id);

  data->set_needs_local_transform_update(needs_local_transform_update);

  data->set_is_invertible(is_invertible);
  data->set_ancestors_are_invertible(ancestors_are_invertible);

  data->set_is_animated(is_animated);
  data->set_to_screen_is_animated(to_screen_is_animated);
  data->set_has_only_translation_animations(has_only_translation_animations);
  data->set_to_screen_has_scale_animation(to_screen_has_scale_animation);

  data->set_flattens_inherited_transform(flattens_inherited_transform);
  data->set_node_and_ancestors_are_flat(node_and_ancestors_are_flat);

  data->set_node_and_ancestors_have_only_integer_translation(
      node_and_ancestors_have_only_integer_translation);
  data->set_scrolls(scrolls);
  data->set_needs_sublayer_scale(needs_sublayer_scale);

  data->set_affected_by_inner_viewport_bounds_delta_x(
      affected_by_inner_viewport_bounds_delta_x);
  data->set_affected_by_inner_viewport_bounds_delta_y(
      affected_by_inner_viewport_bounds_delta_y);
  data->set_affected_by_outer_viewport_bounds_delta_x(
      affected_by_outer_viewport_bounds_delta_x);
  data->set_affected_by_outer_viewport_bounds_delta_y(
      affected_by_outer_viewport_bounds_delta_y);

  data->set_in_subtree_of_page_scale_layer(in_subtree_of_page_scale_layer);
  data->set_post_local_scale_factor(post_local_scale_factor);
  data->set_local_maximum_animation_target_scale(
      local_maximum_animation_target_scale);
  data->set_local_starting_animation_scale(local_starting_animation_scale);
  data->set_combined_maximum_animation_target_scale(
      combined_maximum_animation_target_scale);
  data->set_combined_starting_animation_scale(
      combined_starting_animation_scale);

  Vector2dFToProto(sublayer_scale, data->mutable_sublayer_scale());
  ScrollOffsetToProto(scroll_offset, data->mutable_scroll_offset());
  Vector2dFToProto(scroll_snap, data->mutable_scroll_snap());
  Vector2dFToProto(source_offset, data->mutable_source_offset());
  Vector2dFToProto(source_to_parent, data->mutable_source_to_parent());
}

void TransformNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_transform_node_data());
  const proto::TranformNodeData& data = proto.transform_node_data();

  pre_local = ProtoToTransform(data.pre_local());
  local = ProtoToTransform(data.local());
  post_local = ProtoToTransform(data.post_local());

  to_parent = ProtoToTransform(data.to_parent());

  to_target = ProtoToTransform(data.to_target());
  from_target = ProtoToTransform(data.from_target());

  to_screen = ProtoToTransform(data.to_screen());
  from_screen = ProtoToTransform(data.from_screen());

  target_id = data.target_id();
  content_target_id = data.content_target_id();
  source_node_id = data.source_node_id();
  sorting_context_id = data.sorting_context_id();

  needs_local_transform_update = data.needs_local_transform_update();

  is_invertible = data.is_invertible();
  ancestors_are_invertible = data.ancestors_are_invertible();

  is_animated = data.is_animated();
  to_screen_is_animated = data.to_screen_is_animated();
  has_only_translation_animations = data.has_only_translation_animations();
  to_screen_has_scale_animation = data.to_screen_has_scale_animation();

  flattens_inherited_transform = data.flattens_inherited_transform();
  node_and_ancestors_are_flat = data.node_and_ancestors_are_flat();

  node_and_ancestors_have_only_integer_translation =
      data.node_and_ancestors_have_only_integer_translation();
  scrolls = data.scrolls();
  needs_sublayer_scale = data.needs_sublayer_scale();

  affected_by_inner_viewport_bounds_delta_x =
      data.affected_by_inner_viewport_bounds_delta_x();
  affected_by_inner_viewport_bounds_delta_y =
      data.affected_by_inner_viewport_bounds_delta_y();
  affected_by_outer_viewport_bounds_delta_x =
      data.affected_by_outer_viewport_bounds_delta_x();
  affected_by_outer_viewport_bounds_delta_y =
      data.affected_by_outer_viewport_bounds_delta_y();

  in_subtree_of_page_scale_layer = data.in_subtree_of_page_scale_layer();
  post_local_scale_factor = data.post_local_scale_factor();
  local_maximum_animation_target_scale =
      data.local_maximum_animation_target_scale();
  local_starting_animation_scale = data.local_starting_animation_scale();
  combined_maximum_animation_target_scale =
      data.combined_maximum_animation_target_scale();
  combined_starting_animation_scale = data.combined_starting_animation_scale();

  sublayer_scale = ProtoToVector2dF(data.sublayer_scale());
  scroll_offset = ProtoToScrollOffset(data.scroll_offset());
  scroll_snap = ProtoToVector2dF(data.scroll_snap());
  source_offset = ProtoToVector2dF(data.source_offset());
  source_to_parent = ProtoToVector2dF(data.source_to_parent());
}

ClipNodeData::ClipNodeData()
    : transform_id(-1),
      target_id(-1),
      applies_local_clip(true),
      layer_clipping_uses_only_local_clip(false),
      target_is_clipped(false),
      layers_are_clipped(false),
      layers_are_clipped_when_surfaces_disabled(false),
      resets_clip(false) {}

bool ClipNodeData::operator==(const ClipNodeData& other) const {
  return clip == other.clip &&
         combined_clip_in_target_space == other.combined_clip_in_target_space &&
         clip_in_target_space == other.clip_in_target_space &&
         transform_id == other.transform_id && target_id == other.target_id &&
         applies_local_clip == other.applies_local_clip &&
         layer_clipping_uses_only_local_clip ==
             other.layer_clipping_uses_only_local_clip &&
         target_is_clipped == other.target_is_clipped &&
         layers_are_clipped == other.layers_are_clipped &&
         layers_are_clipped_when_surfaces_disabled ==
             other.layers_are_clipped_when_surfaces_disabled &&
         resets_clip == other.resets_clip;
}

void ClipNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_clip_node_data());
  proto::ClipNodeData* data = proto->mutable_clip_node_data();

  RectFToProto(clip, data->mutable_clip());
  RectFToProto(combined_clip_in_target_space,
               data->mutable_combined_clip_in_target_space());
  RectFToProto(clip_in_target_space, data->mutable_clip_in_target_space());

  data->set_transform_id(transform_id);
  data->set_target_id(target_id);
  data->set_applies_local_clip(applies_local_clip);
  data->set_layer_clipping_uses_only_local_clip(
      layer_clipping_uses_only_local_clip);
  data->set_target_is_clipped(target_is_clipped);
  data->set_layers_are_clipped(layers_are_clipped);
  data->set_layers_are_clipped_when_surfaces_disabled(
      layers_are_clipped_when_surfaces_disabled);
  data->set_resets_clip(resets_clip);
}

void ClipNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_clip_node_data());
  const proto::ClipNodeData& data = proto.clip_node_data();

  clip = ProtoToRectF(data.clip());
  combined_clip_in_target_space =
      ProtoToRectF(data.combined_clip_in_target_space());
  clip_in_target_space = ProtoToRectF(data.clip_in_target_space());

  transform_id = data.transform_id();
  target_id = data.target_id();
  applies_local_clip = data.applies_local_clip();
  layer_clipping_uses_only_local_clip =
      data.layer_clipping_uses_only_local_clip();
  target_is_clipped = data.target_is_clipped();
  layers_are_clipped = data.layers_are_clipped();
  layers_are_clipped_when_surfaces_disabled =
      data.layers_are_clipped_when_surfaces_disabled();
  resets_clip = data.resets_clip();
}

EffectNodeData::EffectNodeData()
    : opacity(1.f),
      screen_space_opacity(1.f),
      has_render_surface(false),
      has_copy_request(false),
      has_background_filters(false),
      is_drawn(true),
      screen_space_opacity_is_animating(false),
      num_copy_requests_in_subtree(0),
      transform_id(0),
      clip_id(0) {}

bool EffectNodeData::operator==(const EffectNodeData& other) const {
  return opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         has_render_surface == other.has_render_surface &&
         has_copy_request == other.has_copy_request &&
         has_background_filters == other.has_background_filters &&
         is_drawn == other.is_drawn &&
         screen_space_opacity_is_animating ==
             other.screen_space_opacity_is_animating &&
         num_copy_requests_in_subtree == other.num_copy_requests_in_subtree &&
         transform_id == other.transform_id && clip_id == other.clip_id;
}

void EffectNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_effect_node_data());
  proto::EffectNodeData* data = proto->mutable_effect_node_data();
  data->set_opacity(opacity);
  data->set_screen_space_opacity(screen_space_opacity);
  data->set_has_render_surface(has_render_surface);
  data->set_has_copy_request(has_copy_request);
  data->set_has_background_filters(has_background_filters);
  data->set_is_drawn(is_drawn);
  data->set_screen_space_opacity_is_animating(
      screen_space_opacity_is_animating);
  data->set_num_copy_requests_in_subtree(num_copy_requests_in_subtree);
  data->set_transform_id(transform_id);
  data->set_clip_id(clip_id);
}

void EffectNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_effect_node_data());
  const proto::EffectNodeData& data = proto.effect_node_data();

  opacity = data.opacity();
  screen_space_opacity = data.screen_space_opacity();
  has_render_surface = data.has_render_surface();
  has_copy_request = data.has_copy_request();
  has_background_filters = data.has_background_filters();
  is_drawn = data.is_drawn();
  screen_space_opacity_is_animating = data.screen_space_opacity_is_animating();
  num_copy_requests_in_subtree = data.num_copy_requests_in_subtree();
  transform_id = data.transform_id();
  clip_id = data.clip_id();
}

ScrollNodeData::ScrollNodeData()
    : scrollable(false),
      should_scroll_on_main_thread(false),
      contains_non_fast_scrollable_region(false),
      transform_id(0) {}

bool ScrollNodeData::operator==(const ScrollNodeData& other) const {
  return scrollable == other.scrollable &&
         should_scroll_on_main_thread == other.should_scroll_on_main_thread &&
         contains_non_fast_scrollable_region ==
             other.contains_non_fast_scrollable_region &&
         transform_id == other.transform_id;
}

void ScrollNodeData::ToProtobuf(proto::TreeNode* proto) const {
  DCHECK(!proto->has_scroll_node_data());
  proto::ScrollNodeData* data = proto->mutable_scroll_node_data();
  data->set_scrollable(scrollable);
  data->set_should_scroll_on_main_thread(should_scroll_on_main_thread);
  data->set_contains_non_fast_scrollable_region(
      contains_non_fast_scrollable_region);
  data->set_transform_id(transform_id);
}

void ScrollNodeData::FromProtobuf(const proto::TreeNode& proto) {
  DCHECK(proto.has_scroll_node_data());
  const proto::ScrollNodeData& data = proto.scroll_node_data();

  scrollable = data.scrollable();
  should_scroll_on_main_thread = data.should_scroll_on_main_thread();
  contains_non_fast_scrollable_region =
      data.contains_non_fast_scrollable_region();
  transform_id = data.transform_id();
}

void TransformTree::clear() {
  PropertyTree<TransformNode>::clear();

  nodes_affected_by_inner_viewport_bounds_delta_.clear();
  nodes_affected_by_outer_viewport_bounds_delta_.clear();
}

bool TransformTree::ComputeTransform(int source_id,
                                     int dest_id,
                                     gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (source_id == dest_id)
    return true;

  if (source_id > dest_id) {
    return CombineTransformsBetween(source_id, dest_id, transform);
  }

  return CombineInversesBetween(source_id, dest_id, transform);
}

bool TransformTree::ComputeTransformWithDestinationSublayerScale(
    int source_id,
    int dest_id,
    gfx::Transform* transform) const {
  bool success = ComputeTransform(source_id, dest_id, transform);

  const TransformNode* dest_node = Node(dest_id);
  if (!dest_node->data.needs_sublayer_scale)
    return success;

  transform->matrix().postScale(dest_node->data.sublayer_scale.x(),
                                dest_node->data.sublayer_scale.y(), 1.f);
  return success;
}

bool TransformTree::ComputeTransformWithSourceSublayerScale(
    int source_id,
    int dest_id,
    gfx::Transform* transform) const {
  bool success = ComputeTransform(source_id, dest_id, transform);

  const TransformNode* source_node = Node(source_id);
  if (!source_node->data.needs_sublayer_scale)
    return success;

  if (source_node->data.sublayer_scale.x() == 0 ||
      source_node->data.sublayer_scale.y() == 0)
    return false;

  transform->Scale(1.f / source_node->data.sublayer_scale.x(),
                   1.f / source_node->data.sublayer_scale.y());
  return success;
}

bool TransformTree::Are2DAxisAligned(int source_id, int dest_id) const {
  gfx::Transform transform;
  return ComputeTransform(source_id, dest_id, &transform) &&
         transform.Preserves2dAxisAlignment();
}

bool TransformTree::NeedsSourceToParentUpdate(TransformNode* node) {
  return (source_to_parent_updates_allowed() &&
          node->parent_id != node->data.source_node_id);
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  TransformNode* target_node = Node(node->data.target_id);
  if (node->data.needs_local_transform_update ||
      NeedsSourceToParentUpdate(node))
    UpdateLocalTransform(node);
  else
    UndoSnapping(node);
  UpdateScreenSpaceTransform(node, parent_node, target_node);
  UpdateSublayerScale(node);
  UpdateTargetSpaceTransform(node, target_node);
  UpdateAnimationProperties(node, parent_node);
  UpdateSnapping(node);
  UpdateNodeAndAncestorsHaveIntegerTranslations(node, parent_node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id < 0)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

bool TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
  DCHECK(source_id > dest_id);
  const TransformNode* current = Node(source_id);
  const TransformNode* dest = Node(dest_id);
  // Combine transforms to and from the screen when possible. Since flattening
  // is a non-linear operation, we cannot use this approach when there is
  // non-trivial flattening between the source and destination nodes. For
  // example, consider the tree R->A->B->C, where B flattens its inherited
  // transform, and A has a non-flat transform. Suppose C is the source and A is
  // the destination. The expected result is C * B. But C's to_screen
  // transform is C * B * flattened(A * R), and A's from_screen transform is
  // R^{-1} * A^{-1}. If at least one of A and R isn't flat, the inverse of
  // flattened(A * R) won't be R^{-1} * A{-1}, so multiplying C's to_screen and
  // A's from_screen will not produce the correct result.
  if (!dest || (dest->data.ancestors_are_invertible &&
                dest->data.node_and_ancestors_are_flat)) {
    transform->ConcatTransform(current->data.to_screen);
    if (dest)
      transform->ConcatTransform(dest->data.from_screen);
    return true;
  }

  // Flattening is defined in a way that requires it to be applied while
  // traversing downward in the tree. We first identify nodes that are on the
  // path from the source to the destination (this is traversing upward), and
  // then we visit these nodes in reverse order, flattening as needed. We
  // early-out if we get to a node whose target node is the destination, since
  // we can then re-use the target space transform stored at that node. However,
  // we cannot re-use a stored target space transform if the destination has a
  // zero sublayer scale, since stored target space transforms have sublayer
  // scale baked in, but we need to compute an unscaled transform.
  std::vector<int> source_to_destination;
  source_to_destination.push_back(current->id);
  current = parent(current);
  bool destination_has_non_zero_sublayer_scale =
      dest->data.sublayer_scale.x() != 0.f &&
      dest->data.sublayer_scale.y() != 0.f;
  DCHECK(destination_has_non_zero_sublayer_scale ||
         !dest->data.ancestors_are_invertible);
  for (; current && current->id > dest_id; current = parent(current)) {
    if (destination_has_non_zero_sublayer_scale &&
        current->data.target_id == dest_id &&
        current->data.content_target_id == dest_id)
      break;
    source_to_destination.push_back(current->id);
  }

  gfx::Transform combined_transform;
  if (current->id > dest_id) {
    combined_transform = current->data.to_target;
    // The stored target space transform has sublayer scale baked in, but we
    // need the unscaled transform.
    combined_transform.matrix().postScale(1.0f / dest->data.sublayer_scale.x(),
                                          1.0f / dest->data.sublayer_scale.y(),
                                          1.0f);
  } else if (current->id < dest_id) {
    // We have reached the lowest common ancestor of the source and destination
    // nodes. This case can occur when we are transforming between a node
    // corresponding to a fixed-position layer (or its descendant) and the node
    // corresponding to the layer's render target. For example, consider the
    // layer tree R->T->S->F where F is fixed-position, S owns a render surface,
    // and T has a significant transform. This will yield the following
    // transform tree:
    //    R
    //    |
    //    T
    //   /|
    //  S F
    // In this example, T will have id 2, S will have id 3, and F will have id
    // 4. When walking up the ancestor chain from F, the first node with a
    // smaller id than S will be T, the lowest common ancestor of these nodes.
    // We compute the transform from T to S here, and then from F to T in the
    // loop below.
    DCHECK(IsDescendant(dest_id, current->id));
    CombineInversesBetween(current->id, dest_id, &combined_transform);
    DCHECK(combined_transform.IsApproximatelyIdentityOrTranslation(
        SkDoubleToMScalar(1e-4)));
  }

  size_t source_to_destination_size = source_to_destination.size();
  for (size_t i = 0; i < source_to_destination_size; ++i) {
    size_t index = source_to_destination_size - 1 - i;
    const TransformNode* node = Node(source_to_destination[index]);
    if (node->data.flattens_inherited_transform)
      combined_transform.FlattenTo2d();
    combined_transform.PreconcatTransform(node->data.to_parent);
  }

  transform->ConcatTransform(combined_transform);
  return true;
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  DCHECK(source_id < dest_id);
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  // Just as in CombineTransformsBetween, we can use screen space transforms in
  // this computation only when there isn't any non-trivial flattening
  // involved.
  if (current->data.ancestors_are_invertible &&
      current->data.node_and_ancestors_are_flat) {
    transform->PreconcatTransform(current->data.from_screen);
    if (dest)
      transform->PreconcatTransform(dest->data.to_screen);
    return true;
  }

  // Inverting a flattening is not equivalent to flattening an inverse. This
  // means we cannot, for example, use the inverse of each node's to_parent
  // transform, flattening where needed. Instead, we must compute the transform
  // from the destination to the source, with flattening, and then invert the
  // result.
  gfx::Transform dest_to_source;
  CombineTransformsBetween(dest_id, source_id, &dest_to_source);
  gfx::Transform source_to_dest;
  bool all_are_invertible = dest_to_source.GetInverse(&source_to_dest);
  transform->PreconcatTransform(source_to_dest);
  return all_are_invertible;
}

void TransformTree::UpdateLocalTransform(TransformNode* node) {
  gfx::Transform transform = node->data.post_local;
  if (NeedsSourceToParentUpdate(node)) {
    gfx::Transform to_parent;
    ComputeTransform(node->data.source_node_id, node->parent_id, &to_parent);
    node->data.source_to_parent = to_parent.To2dTranslation();
  }

  gfx::Vector2dF fixed_position_adjustment;
  if (node->data.affected_by_inner_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(inner_viewport_bounds_delta_.x());
  else if (node->data.affected_by_outer_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(outer_viewport_bounds_delta_.x());

  if (node->data.affected_by_inner_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(inner_viewport_bounds_delta_.y());
  else if (node->data.affected_by_outer_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(outer_viewport_bounds_delta_.y());

  transform.Translate(
      node->data.source_to_parent.x() - node->data.scroll_offset.x() +
          fixed_position_adjustment.x(),
      node->data.source_to_parent.y() - node->data.scroll_offset.y() +
          fixed_position_adjustment.y());
  transform.PreconcatTransform(node->data.local);
  transform.PreconcatTransform(node->data.pre_local);
  node->data.set_to_parent(transform);
  node->data.needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node,
                                               TransformNode* target_node) {
  if (!parent_node) {
    node->data.to_screen = node->data.to_parent;
    node->data.ancestors_are_invertible = true;
    node->data.to_screen_is_animated = false;
    node->data.node_and_ancestors_are_flat = node->data.to_parent.IsFlat();
  } else {
    node->data.to_screen = parent_node->data.to_screen;
    if (node->data.flattens_inherited_transform)
      node->data.to_screen.FlattenTo2d();
    node->data.to_screen.PreconcatTransform(node->data.to_parent);
    node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
    node->data.node_and_ancestors_are_flat =
        parent_node->data.node_and_ancestors_are_flat &&
        node->data.to_parent.IsFlat();
  }

  if (!node->data.to_screen.GetInverse(&node->data.from_screen))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateSublayerScale(TransformNode* node) {
  // The sublayer scale depends on the screen space transform, so update it too.
  if (!node->data.needs_sublayer_scale) {
    node->data.sublayer_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  float layer_scale_factor =
      device_scale_factor_ * device_transform_scale_factor_;
  if (node->data.in_subtree_of_page_scale_layer)
    layer_scale_factor *= page_scale_factor_;
  node->data.sublayer_scale = MathUtil::ComputeTransform2dScaleComponents(
      node->data.to_screen, layer_scale_factor);
}

void TransformTree::UpdateTargetSpaceTransform(TransformNode* node,
                                               TransformNode* target_node) {
  if (node->data.needs_sublayer_scale) {
    node->data.to_target.MakeIdentity();
    node->data.to_target.Scale(node->data.sublayer_scale.x(),
                               node->data.sublayer_scale.y());
  } else {
    // In order to include the root transform for the root surface, we walk up
    // to the root of the transform tree in ComputeTransform.
    int target_id = target_node->id;
    ComputeTransformWithDestinationSublayerScale(node->id, target_id,
                                                 &node->data.to_target);
  }

  if (!node->data.to_target.GetInverse(&node->data.from_target))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateAnimationProperties(TransformNode* node,
                                              TransformNode* parent_node) {
  bool ancestor_is_animating = false;
  bool ancestor_is_animating_scale = false;
  float ancestor_maximum_target_scale = 0.f;
  float ancestor_starting_animation_scale = 0.f;
  if (parent_node) {
    ancestor_is_animating = parent_node->data.to_screen_is_animated;
    ancestor_is_animating_scale =
        parent_node->data.to_screen_has_scale_animation;
    ancestor_maximum_target_scale =
        parent_node->data.combined_maximum_animation_target_scale;
    ancestor_starting_animation_scale =
        parent_node->data.combined_starting_animation_scale;
  }
  node->data.to_screen_is_animated =
      node->data.is_animated || ancestor_is_animating;
  node->data.to_screen_has_scale_animation =
      !node->data.has_only_translation_animations ||
      ancestor_is_animating_scale;

  // Once we've failed to compute a maximum animated scale at an ancestor, we
  // continue to fail.
  bool failed_at_ancestor =
      ancestor_is_animating_scale && ancestor_maximum_target_scale == 0.f;

  // Computing maximum animated scale in the presence of non-scale/translation
  // transforms isn't supported.
  bool failed_for_non_scale_or_translation =
      !node->data.to_target.IsScaleOrTranslation();

  // We don't attempt to accumulate animation scale from multiple nodes with
  // scale animations, because of the risk of significant overestimation. For
  // example, one node might be increasing scale from 1 to 10 at the same time
  // as another node is decreasing scale from 10 to 1. Naively combining these
  // scales would produce a scale of 100.
  bool failed_for_multiple_scale_animations =
      ancestor_is_animating_scale &&
      !node->data.has_only_translation_animations;

  if (failed_at_ancestor || failed_for_non_scale_or_translation ||
      failed_for_multiple_scale_animations) {
    node->data.combined_maximum_animation_target_scale = 0.f;
    node->data.combined_starting_animation_scale = 0.f;

    // This ensures that descendants know we've failed to compute a maximum
    // animated scale.
    node->data.to_screen_has_scale_animation = true;
    return;
  }

  if (!node->data.to_screen_has_scale_animation) {
    node->data.combined_maximum_animation_target_scale = 0.f;
    node->data.combined_starting_animation_scale = 0.f;
    return;
  }

  // At this point, we know exactly one of this node or an ancestor is animating
  // scale.
  if (node->data.has_only_translation_animations) {
    // An ancestor is animating scale.
    gfx::Vector2dF local_scales =
        MathUtil::ComputeTransform2dScaleComponents(node->data.local, 0.f);
    float max_local_scale = std::max(local_scales.x(), local_scales.y());
    node->data.combined_maximum_animation_target_scale =
        max_local_scale * ancestor_maximum_target_scale;
    node->data.combined_starting_animation_scale =
        max_local_scale * ancestor_starting_animation_scale;
    return;
  }

  if (node->data.local_starting_animation_scale == 0.f ||
      node->data.local_maximum_animation_target_scale == 0.f) {
    node->data.combined_maximum_animation_target_scale = 0.f;
    node->data.combined_starting_animation_scale = 0.f;
    return;
  }

  gfx::Vector2dF ancestor_scales =
      parent_node ? MathUtil::ComputeTransform2dScaleComponents(
                        parent_node->data.to_target, 0.f)
                  : gfx::Vector2dF(1.f, 1.f);
  float max_ancestor_scale = std::max(ancestor_scales.x(), ancestor_scales.y());
  node->data.combined_maximum_animation_target_scale =
      max_ancestor_scale * node->data.local_maximum_animation_target_scale;
  node->data.combined_starting_animation_scale =
      max_ancestor_scale * node->data.local_starting_animation_scale;
}

void TransformTree::UndoSnapping(TransformNode* node) {
  // to_parent transform has the scroll snap from previous frame baked in.
  // We need to undo it and use the un-snapped transform to compute current
  // target and screen space transforms.
  node->data.to_parent.Translate(-node->data.scroll_snap.x(),
                                 -node->data.scroll_snap.y());
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->data.scrolls || node->data.to_screen_is_animated ||
      !node->data.to_target.IsScaleOrTranslation() ||
      !node->data.ancestors_are_invertible) {
    return;
  }

  // Scroll snapping must be done in target space (the pixels we care about).
  // This means we effectively snap the target space transform. If TT is the
  // target space transform and TT' is TT with its translation components
  // rounded, then what we're after is the scroll delta X, where TT * X = TT'.
  // I.e., we want a transform that will realize our scroll snap. It follows
  // that X = TT^-1 * TT'. We cache TT and TT^-1 to make this more efficient.
  gfx::Transform rounded = node->data.to_target;
  rounded.RoundTranslationComponents();
  gfx::Transform delta = node->data.from_target;
  delta *= rounded;

  DCHECK(delta.IsApproximatelyIdentityOrTranslation(SkDoubleToMScalar(1e-4)))
      << delta.ToString();

  gfx::Vector2dF translation = delta.To2dTranslation();

  // Now that we have our scroll delta, we must apply it to each of our
  // combined, to/from matrices.
  node->data.to_target = rounded;
  node->data.to_parent.Translate(translation.x(), translation.y());
  node->data.from_target.matrix().postTranslate(-translation.x(),
                                                -translation.y(), 0);
  node->data.to_screen.Translate(translation.x(), translation.y());
  node->data.from_screen.matrix().postTranslate(-translation.x(),
                                                -translation.y(), 0);

  node->data.scroll_snap = translation;
}

void TransformTree::SetDeviceTransform(const gfx::Transform& transform,
                                       gfx::PointF root_position) {
  gfx::Transform root_post_local = transform;
  TransformNode* node = Node(1);
  root_post_local.Scale(node->data.post_local_scale_factor,
                        node->data.post_local_scale_factor);
  root_post_local.Translate(root_position.x(), root_position.y());
  if (node->data.post_local == root_post_local)
    return;

  node->data.post_local = root_post_local;
  node->data.needs_local_transform_update = true;
  set_needs_update(true);
}

void TransformTree::SetDeviceTransformScaleFactor(
    const gfx::Transform& transform) {
  gfx::Vector2dF device_transform_scale_components =
      MathUtil::ComputeTransform2dScaleComponents(transform, 1.f);

  // Not handling the rare case of different x and y device scale.
  device_transform_scale_factor_ =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());
}

void TransformTree::SetInnerViewportBoundsDelta(gfx::Vector2dF bounds_delta) {
  if (inner_viewport_bounds_delta_ == bounds_delta)
    return;

  inner_viewport_bounds_delta_ = bounds_delta;

  if (nodes_affected_by_inner_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_inner_viewport_bounds_delta_)
    Node(i)->data.needs_local_transform_update = true;
}

void TransformTree::SetOuterViewportBoundsDelta(gfx::Vector2dF bounds_delta) {
  if (outer_viewport_bounds_delta_ == bounds_delta)
    return;

  outer_viewport_bounds_delta_ = bounds_delta;

  if (nodes_affected_by_outer_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_outer_viewport_bounds_delta_)
    Node(i)->data.needs_local_transform_update = true;
}

void TransformTree::AddNodeAffectedByInnerViewportBoundsDelta(int node_id) {
  nodes_affected_by_inner_viewport_bounds_delta_.push_back(node_id);
}

void TransformTree::AddNodeAffectedByOuterViewportBoundsDelta(int node_id) {
  nodes_affected_by_outer_viewport_bounds_delta_.push_back(node_id);
}

bool TransformTree::HasNodesAffectedByInnerViewportBoundsDelta() const {
  return !nodes_affected_by_inner_viewport_bounds_delta_.empty();
}

bool TransformTree::HasNodesAffectedByOuterViewportBoundsDelta() const {
  return !nodes_affected_by_outer_viewport_bounds_delta_.empty();
}

bool TransformTree::operator==(const TransformTree& other) const {
  return PropertyTree::operator==(other) &&
         source_to_parent_updates_allowed_ ==
             other.source_to_parent_updates_allowed() &&
         page_scale_factor_ == other.page_scale_factor() &&
         device_scale_factor_ == other.device_scale_factor() &&
         device_transform_scale_factor_ ==
             other.device_transform_scale_factor() &&
         inner_viewport_bounds_delta_ == other.inner_viewport_bounds_delta() &&
         outer_viewport_bounds_delta_ == other.outer_viewport_bounds_delta() &&
         nodes_affected_by_inner_viewport_bounds_delta_ ==
             other.nodes_affected_by_inner_viewport_bounds_delta() &&
         nodes_affected_by_outer_viewport_bounds_delta_ ==
             other.nodes_affected_by_outer_viewport_bounds_delta();
}

void TransformTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Transform);

  PropertyTree::ToProtobuf(proto);
  proto::TransformTreeData* data = proto->mutable_transform_tree_data();

  data->set_source_to_parent_updates_allowed(source_to_parent_updates_allowed_);
  data->set_page_scale_factor(page_scale_factor_);
  data->set_device_scale_factor(device_scale_factor_);
  data->set_device_transform_scale_factor(device_transform_scale_factor_);

  Vector2dFToProto(inner_viewport_bounds_delta_,
                   data->mutable_inner_viewport_bounds_delta());
  Vector2dFToProto(outer_viewport_bounds_delta_,
                   data->mutable_outer_viewport_bounds_delta());

  for (auto i : nodes_affected_by_inner_viewport_bounds_delta_)
    data->add_nodes_affected_by_inner_viewport_bounds_delta(i);

  for (auto i : nodes_affected_by_outer_viewport_bounds_delta_)
    data->add_nodes_affected_by_outer_viewport_bounds_delta(i);
}

void TransformTree::FromProtobuf(const proto::PropertyTree& proto) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Transform);

  PropertyTree::FromProtobuf(proto);
  const proto::TransformTreeData& data = proto.transform_tree_data();

  source_to_parent_updates_allowed_ = data.source_to_parent_updates_allowed();
  page_scale_factor_ = data.page_scale_factor();
  device_scale_factor_ = data.device_scale_factor();
  device_transform_scale_factor_ = data.device_transform_scale_factor();

  inner_viewport_bounds_delta_ =
      ProtoToVector2dF(data.inner_viewport_bounds_delta());
  outer_viewport_bounds_delta_ =
      ProtoToVector2dF(data.outer_viewport_bounds_delta());

  DCHECK(nodes_affected_by_inner_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_inner_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_inner_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_inner_viewport_bounds_delta(i));
  }

  DCHECK(nodes_affected_by_outer_viewport_bounds_delta_.empty());
  for (int i = 0; i < data.nodes_affected_by_outer_viewport_bounds_delta_size();
       ++i) {
    nodes_affected_by_outer_viewport_bounds_delta_.push_back(
        data.nodes_affected_by_outer_viewport_bounds_delta(i));
  }
}

void EffectTree::UpdateOpacities(EffectNode* node, EffectNode* parent_node) {
  node->data.screen_space_opacity = node->data.opacity;

  if (parent_node)
    node->data.screen_space_opacity *= parent_node->data.screen_space_opacity;
}

void EffectTree::UpdateIsDrawn(EffectNode* node, EffectNode* parent_node) {
  // Nodes that have screen space opacity 0 are hidden. So they are not drawn.
  // Exceptions:
  // 1) Nodes that contribute to copy requests, whether hidden or not, must be
  //    drawn.
  // 2) Nodes that have a background filter.
  // 3) Nodes with animating screen space opacity are drawn if their parent is
  //    drawn irrespective of their opacity.
  if (node->data.has_copy_request || node->data.has_background_filters)
    node->data.is_drawn = true;
  else if (node->data.screen_space_opacity_is_animating)
    node->data.is_drawn = parent_node ? parent_node->data.is_drawn : true;
  else if (node->data.opacity == 0.f)
    node->data.is_drawn = false;
  else if (parent_node)
    node->data.is_drawn = parent_node->data.is_drawn;
  else
    node->data.is_drawn = true;
}

void EffectTree::UpdateEffects(int id) {
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);

  UpdateOpacities(node, parent_node);
  UpdateIsDrawn(node, parent_node);
}

void EffectTree::ClearCopyRequests() {
  for (auto& node : nodes()) {
    node.data.num_copy_requests_in_subtree = 0;
    node.data.has_copy_request = false;
  }
  set_needs_update(true);
}

bool EffectTree::ContributesToDrawnSurface(int id) {
  // All drawn nodes contribute to drawn surface.
  // Exception : Nodes that are hidden and are drawn only for the sake of
  // copy requests.
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);
  bool contributes_to_drawn_surface =
      node->data.is_drawn &&
      (node->data.opacity != 0.f || node->data.has_background_filters);
  if (parent_node && !parent_node->data.is_drawn)
    contributes_to_drawn_surface = false;
  return contributes_to_drawn_surface;
}

void TransformTree::UpdateNodeAndAncestorsHaveIntegerTranslations(
    TransformNode* node,
    TransformNode* parent_node) {
  node->data.node_and_ancestors_have_only_integer_translation =
      node->data.to_parent.IsIdentityOrIntegerTranslation();
  if (parent_node)
    node->data.node_and_ancestors_have_only_integer_translation =
        node->data.node_and_ancestors_have_only_integer_translation &&
        parent_node->data.node_and_ancestors_have_only_integer_translation;
}

void ClipTree::SetViewportClip(gfx::RectF viewport_rect) {
  if (size() < 2)
    return;
  ClipNode* node = Node(1);
  if (viewport_rect == node->data.clip)
    return;
  node->data.clip = viewport_rect;
  set_needs_update(true);
}

gfx::RectF ClipTree::ViewportClip() {
  const unsigned long min_size = 1;
  DCHECK_GT(size(), min_size);
  return Node(1)->data.clip;
}

bool ClipTree::operator==(const ClipTree& other) const {
  return PropertyTree::operator==(other);
}

void ClipTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Clip);

  PropertyTree::ToProtobuf(proto);
}

void ClipTree::FromProtobuf(const proto::PropertyTree& proto) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Clip);

  PropertyTree::FromProtobuf(proto);
}

bool EffectTree::operator==(const EffectTree& other) const {
  return PropertyTree::operator==(other);
}

void EffectTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Effect);

  PropertyTree::ToProtobuf(proto);
}

void EffectTree::FromProtobuf(const proto::PropertyTree& proto) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Effect);

  PropertyTree::FromProtobuf(proto);
}

bool ScrollTree::operator==(const ScrollTree& other) const {
  return PropertyTree::operator==(other);
}

void ScrollTree::ToProtobuf(proto::PropertyTree* proto) const {
  DCHECK(!proto->has_property_type());
  proto->set_property_type(proto::PropertyTree::Scroll);

  PropertyTree::ToProtobuf(proto);
}

void ScrollTree::FromProtobuf(const proto::PropertyTree& proto) {
  DCHECK(proto.has_property_type());
  DCHECK_EQ(proto.property_type(), proto::PropertyTree::Scroll);

  PropertyTree::FromProtobuf(proto);
}

PropertyTrees::PropertyTrees()
    : needs_rebuild(true),
      non_root_surfaces_enabled(true),
      sequence_number(0) {}

PropertyTrees::~PropertyTrees() {}

bool PropertyTrees::operator==(const PropertyTrees& other) const {
  return transform_tree == other.transform_tree &&
         effect_tree == other.effect_tree && clip_tree == other.clip_tree &&
         scroll_tree == other.scroll_tree &&
         needs_rebuild == other.needs_rebuild &&
         non_root_surfaces_enabled == other.non_root_surfaces_enabled &&
         sequence_number == other.sequence_number;
}

void PropertyTrees::ToProtobuf(proto::PropertyTrees* proto) const {
  // TODO(khushalsagar): Add support for sending diffs when serializaing
  // property trees. See crbug/555370.
  transform_tree.ToProtobuf(proto->mutable_transform_tree());
  effect_tree.ToProtobuf(proto->mutable_effect_tree());
  clip_tree.ToProtobuf(proto->mutable_clip_tree());
  scroll_tree.ToProtobuf(proto->mutable_scroll_tree());
  proto->set_needs_rebuild(needs_rebuild);
  proto->set_non_root_surfaces_enabled(non_root_surfaces_enabled);

  // TODO(khushalsagar): Consider using the sequence number to decide if
  // property trees need to be serialized again for a commit. See crbug/555370.
  proto->set_sequence_number(sequence_number);
}

// static
void PropertyTrees::FromProtobuf(const proto::PropertyTrees& proto) {
  transform_tree.FromProtobuf(proto.transform_tree());
  effect_tree.FromProtobuf(proto.effect_tree());
  clip_tree.FromProtobuf(proto.clip_tree());
  scroll_tree.FromProtobuf(proto.scroll_tree());

  needs_rebuild = proto.needs_rebuild();
  non_root_surfaces_enabled = proto.non_root_surfaces_enabled();
  sequence_number = proto.sequence_number();
}

}  // namespace cc

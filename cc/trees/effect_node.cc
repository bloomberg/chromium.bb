// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/effect_node.h"

namespace cc {

EffectNode::EffectNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      opacity(1.f),
      screen_space_opacity(1.f),
      blend_mode(SkXfermode::kSrcOver_Mode),
      has_render_surface(false),
      render_surface(nullptr),
      has_copy_request(false),
      hidden_by_backface_visibility(false),
      double_sided(false),
      is_drawn(true),
      subtree_hidden(false),
      has_potential_filter_animation(false),
      has_potential_opacity_animation(false),
      is_currently_animating_filter(false),
      is_currently_animating_opacity(false),
      effect_changed(false),
      num_copy_requests_in_subtree(0),
      has_unclipped_descendants(false),
      transform_id(0),
      clip_id(0),
      target_id(0),
      mask_layer_id(-1) {}

EffectNode::EffectNode(const EffectNode& other) = default;

bool EffectNode::operator==(const EffectNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         has_render_surface == other.has_render_surface &&
         has_copy_request == other.has_copy_request &&
         filters == other.filters &&
         background_filters == other.background_filters &&
         filters_origin == other.filters_origin &&
         blend_mode == other.blend_mode &&
         surface_contents_scale == other.surface_contents_scale &&
         unscaled_mask_target_size == other.unscaled_mask_target_size &&
         hidden_by_backface_visibility == other.hidden_by_backface_visibility &&
         double_sided == other.double_sided && is_drawn == other.is_drawn &&
         subtree_hidden == other.subtree_hidden &&
         has_potential_filter_animation ==
             other.has_potential_filter_animation &&
         has_potential_opacity_animation ==
             other.has_potential_opacity_animation &&
         is_currently_animating_filter == other.is_currently_animating_filter &&
         is_currently_animating_opacity ==
             other.is_currently_animating_opacity &&
         effect_changed == other.effect_changed &&
         num_copy_requests_in_subtree == other.num_copy_requests_in_subtree &&
         transform_id == other.transform_id && clip_id == other.clip_id &&
         target_id == other.target_id && mask_layer_id == other.mask_layer_id;
}

void EffectNode::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);

  DCHECK(!proto->has_effect_node_data());
  proto::EffectNodeData* data = proto->mutable_effect_node_data();
  data->set_opacity(opacity);
  data->set_screen_space_opacity(screen_space_opacity);
  data->set_blend_mode(SkXfermodeModeToProto(blend_mode));
  data->set_has_render_surface(has_render_surface);
  data->set_has_copy_request(has_copy_request);
  data->set_hidden_by_backface_visibility(hidden_by_backface_visibility);
  data->set_double_sided(double_sided);
  data->set_is_drawn(is_drawn);
  data->set_subtree_hidden(subtree_hidden);
  data->set_has_potential_filter_animation(has_potential_filter_animation);
  data->set_has_potential_opacity_animation(has_potential_opacity_animation);
  data->set_is_currently_animating_filter(is_currently_animating_filter);
  data->set_is_currently_animating_opacity(is_currently_animating_opacity);
  data->set_effect_changed(effect_changed);
  data->set_num_copy_requests_in_subtree(num_copy_requests_in_subtree);
  data->set_transform_id(transform_id);
  data->set_clip_id(clip_id);
  data->set_target_id(target_id);
  data->set_mask_layer_id(mask_layer_id);
  Vector2dFToProto(surface_contents_scale,
                   data->mutable_surface_contents_scale());
  SizeToProto(unscaled_mask_target_size,
              data->mutable_unscaled_mask_target_size());
}

void EffectNode::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();

  DCHECK(proto.has_effect_node_data());
  const proto::EffectNodeData& data = proto.effect_node_data();

  opacity = data.opacity();
  screen_space_opacity = data.screen_space_opacity();
  blend_mode = SkXfermodeModeFromProto(data.blend_mode());
  unscaled_mask_target_size = ProtoToSize(data.unscaled_mask_target_size());
  has_render_surface = data.has_render_surface();
  has_copy_request = data.has_copy_request();
  hidden_by_backface_visibility = data.hidden_by_backface_visibility();
  double_sided = data.double_sided();
  is_drawn = data.is_drawn();
  subtree_hidden = data.subtree_hidden();
  has_potential_filter_animation = data.has_potential_filter_animation();
  has_potential_opacity_animation = data.has_potential_opacity_animation();
  is_currently_animating_filter = data.is_currently_animating_filter();
  is_currently_animating_opacity = data.is_currently_animating_opacity();
  effect_changed = data.effect_changed();
  num_copy_requests_in_subtree = data.num_copy_requests_in_subtree();
  transform_id = data.transform_id();
  clip_id = data.clip_id();
  target_id = data.target_id();
  mask_layer_id = data.mask_layer_id();
  surface_contents_scale = ProtoToVector2dF(data.surface_contents_scale());
}

void EffectNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  value->SetDouble("opacity", opacity);
  value->SetBoolean("has_render_surface", has_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("has_potential_filter_animation",
                    has_potential_filter_animation);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetInteger("num_copy_requests_in_subtree",
                    num_copy_requests_in_subtree);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("mask_layer_id", mask_layer_id);
}

}  // namespace cc

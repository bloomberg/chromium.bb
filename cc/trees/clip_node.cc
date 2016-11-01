// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/property_tree.h"

namespace cc {

ClipNode::ClipNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      clip_type(ClipType::NONE),
      transform_id(-1),
      target_transform_id(-1),
      target_effect_id(-1),
      layer_clipping_uses_only_local_clip(false),
      target_is_clipped(false),
      layers_are_clipped(false),
      layers_are_clipped_when_surfaces_disabled(false),
      resets_clip(false) {}

ClipNode::ClipNode(const ClipNode& other)
    : id(other.id),
      parent_id(other.parent_id),
      owner_id(other.owner_id),
      clip_type(other.clip_type),
      clip(other.clip),
      combined_clip_in_target_space(other.combined_clip_in_target_space),
      clip_in_target_space(other.clip_in_target_space),
      transform_id(other.transform_id),
      target_transform_id(other.target_transform_id),
      target_effect_id(other.target_effect_id),
      layer_clipping_uses_only_local_clip(
          other.layer_clipping_uses_only_local_clip),
      target_is_clipped(other.target_is_clipped),
      layers_are_clipped(other.layers_are_clipped),
      layers_are_clipped_when_surfaces_disabled(
          other.layers_are_clipped_when_surfaces_disabled),
      resets_clip(other.resets_clip) {
  if (other.clip_expander) {
    DCHECK_EQ(clip_type, ClipType::EXPANDS_CLIP);
    clip_expander = base::MakeUnique<ClipExpander>(*other.clip_expander);
  }
}

ClipNode& ClipNode::operator=(const ClipNode& other) {
  id = other.id;
  parent_id = other.parent_id;
  owner_id = other.owner_id;
  clip_type = other.clip_type;
  clip = other.clip;
  combined_clip_in_target_space = other.combined_clip_in_target_space;
  clip_in_target_space = other.clip_in_target_space;
  transform_id = other.transform_id;
  target_transform_id = other.target_transform_id;
  target_effect_id = other.target_effect_id;
  layer_clipping_uses_only_local_clip =
      other.layer_clipping_uses_only_local_clip;
  target_is_clipped = other.target_is_clipped;
  layers_are_clipped = other.layers_are_clipped;
  layers_are_clipped_when_surfaces_disabled =
      other.layers_are_clipped_when_surfaces_disabled;
  resets_clip = other.resets_clip;

  if (other.clip_expander) {
    DCHECK_EQ(clip_type, ClipType::EXPANDS_CLIP);
    clip_expander = base::MakeUnique<ClipExpander>(*other.clip_expander);
  } else {
    clip_expander.reset();
  }

  return *this;
}

ClipNode::~ClipNode() {}

bool ClipNode::operator==(const ClipNode& other) const {
  if (clip_expander && other.clip_expander &&
      *clip_expander != *other.clip_expander)
    return false;
  if ((clip_expander && !other.clip_expander) ||
      (!clip_expander && other.clip_expander))
    return false;
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && clip_type == other.clip_type &&
         clip == other.clip &&
         combined_clip_in_target_space == other.combined_clip_in_target_space &&
         clip_in_target_space == other.clip_in_target_space &&
         transform_id == other.transform_id &&
         target_transform_id == other.target_transform_id &&
         target_effect_id == other.target_effect_id &&
         layer_clipping_uses_only_local_clip ==
             other.layer_clipping_uses_only_local_clip &&
         target_is_clipped == other.target_is_clipped &&
         layers_are_clipped == other.layers_are_clipped &&
         layers_are_clipped_when_surfaces_disabled ==
             other.layers_are_clipped_when_surfaces_disabled &&
         resets_clip == other.resets_clip;
}

void ClipNode::ToProtobuf(proto::TreeNode* proto) const {
  proto->set_id(id);
  proto->set_parent_id(parent_id);
  proto->set_owner_id(owner_id);

  DCHECK(!proto->has_clip_node_data());
  proto::ClipNodeData* data = proto->mutable_clip_node_data();

  data->set_clip_type(ClipNodeTypeToProto(clip_type));
  if (clip_type == ClipType::EXPANDS_CLIP)
    data->set_clip_expander_effect_id(clip_expander->target_effect_id());
  else
    data->set_clip_expander_effect_id(EffectTree::kInvalidNodeId);

  RectFToProto(clip, data->mutable_clip());
  RectFToProto(combined_clip_in_target_space,
               data->mutable_combined_clip_in_target_space());
  RectFToProto(clip_in_target_space, data->mutable_clip_in_target_space());

  data->set_transform_id(transform_id);
  data->set_target_transform_id(target_transform_id);
  data->set_target_effect_id(target_effect_id);
  data->set_layer_clipping_uses_only_local_clip(
      layer_clipping_uses_only_local_clip);
  data->set_target_is_clipped(target_is_clipped);
  data->set_layers_are_clipped(layers_are_clipped);
  data->set_layers_are_clipped_when_surfaces_disabled(
      layers_are_clipped_when_surfaces_disabled);
  data->set_resets_clip(resets_clip);
}

void ClipNode::FromProtobuf(const proto::TreeNode& proto) {
  id = proto.id();
  parent_id = proto.parent_id();
  owner_id = proto.owner_id();

  DCHECK(proto.has_clip_node_data());
  const proto::ClipNodeData& data = proto.clip_node_data();

  clip_type = ClipNodeTypeFromProto(data.clip_type());
  if (clip_type == ClipNode::ClipType::EXPANDS_CLIP) {
    clip_expander =
        base::MakeUnique<ClipExpander>(data.clip_expander_effect_id());
  } else {
    clip_expander.reset();
  }
  clip = ProtoToRectF(data.clip());
  combined_clip_in_target_space =
      ProtoToRectF(data.combined_clip_in_target_space());
  clip_in_target_space = ProtoToRectF(data.clip_in_target_space());

  transform_id = data.transform_id();
  target_transform_id = data.target_transform_id();
  target_effect_id = data.target_effect_id();
  layer_clipping_uses_only_local_clip =
      data.layer_clipping_uses_only_local_clip();
  target_is_clipped = data.target_is_clipped();
  layers_are_clipped = data.layers_are_clipped();
  layers_are_clipped_when_surfaces_disabled =
      data.layers_are_clipped_when_surfaces_disabled();
  resets_clip = data.resets_clip();
}

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  value->SetInteger("clip_type", static_cast<int>(clip_type));
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("target_transform_id", target_transform_id);
  value->SetInteger("target_effect_id", target_effect_id);
  value->SetBoolean("layer_clipping_uses_only_local_clip",
                    layer_clipping_uses_only_local_clip);
  value->SetBoolean("target_is_clipped", target_is_clipped);
  value->SetBoolean("layers_are_clipped", layers_are_clipped);
  value->SetBoolean("layers_are_clipped_when_surfaces_disabled",
                    layers_are_clipped_when_surfaces_disabled);
  value->SetBoolean("resets_clip", resets_clip);
}

}  // namespace cc

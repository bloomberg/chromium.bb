// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/property_tree.h"

namespace cc {

ClipNode::ClipNode()
    : id(ClipTree::kInvalidNodeId),
      parent_id(ClipTree::kInvalidNodeId),
      owning_layer_id(Layer::INVALID_ID),
      clip_type(ClipType::NONE),
      transform_id(TransformTree::kInvalidNodeId),
      target_transform_id(TransformTree::kInvalidNodeId),
      target_effect_id(EffectTree::kInvalidNodeId),
      layer_clipping_uses_only_local_clip(false),
      layers_are_clipped(false),
      layers_are_clipped_when_surfaces_disabled(false),
      resets_clip(false) {}

ClipNode::ClipNode(const ClipNode& other)
    : id(other.id),
      parent_id(other.parent_id),
      owning_layer_id(other.owning_layer_id),
      clip_type(other.clip_type),
      clip(other.clip),
      combined_clip_in_target_space(other.combined_clip_in_target_space),
      clip_in_target_space(other.clip_in_target_space),
      transform_id(other.transform_id),
      target_transform_id(other.target_transform_id),
      target_effect_id(other.target_effect_id),
      layer_clipping_uses_only_local_clip(
          other.layer_clipping_uses_only_local_clip),
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
  owning_layer_id = other.owning_layer_id;
  clip_type = other.clip_type;
  clip = other.clip;
  combined_clip_in_target_space = other.combined_clip_in_target_space;
  clip_in_target_space = other.clip_in_target_space;
  transform_id = other.transform_id;
  target_transform_id = other.target_transform_id;
  target_effect_id = other.target_effect_id;
  layer_clipping_uses_only_local_clip =
      other.layer_clipping_uses_only_local_clip;
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
         owning_layer_id == other.owning_layer_id &&
         clip_type == other.clip_type && clip == other.clip &&
         combined_clip_in_target_space == other.combined_clip_in_target_space &&
         clip_in_target_space == other.clip_in_target_space &&
         transform_id == other.transform_id &&
         target_transform_id == other.target_transform_id &&
         target_effect_id == other.target_effect_id &&
         layer_clipping_uses_only_local_clip ==
             other.layer_clipping_uses_only_local_clip &&
         layers_are_clipped == other.layers_are_clipped &&
         layers_are_clipped_when_surfaces_disabled ==
             other.layers_are_clipped_when_surfaces_disabled &&
         resets_clip == other.resets_clip;
}

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owning_layer_id", owning_layer_id);
  value->SetInteger("clip_type", static_cast<int>(clip_type));
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("target_transform_id", target_transform_id);
  value->SetInteger("target_effect_id", target_effect_id);
  value->SetBoolean("layer_clipping_uses_only_local_clip",
                    layer_clipping_uses_only_local_clip);
  value->SetBoolean("layers_are_clipped", layers_are_clipped);
  value->SetBoolean("layers_are_clipped_when_surfaces_disabled",
                    layers_are_clipped_when_surfaces_disabled);
  value->SetBoolean("resets_clip", resets_clip);
}

}  // namespace cc

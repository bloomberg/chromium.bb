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
      clip_type(ClipType::APPLIES_LOCAL_CLIP),
      transform_id(TransformTree::kInvalidNodeId) {
}

ClipNode::ClipNode(const ClipNode& other)
    : id(other.id),
      parent_id(other.parent_id),
      clip_type(other.clip_type),
      clip(other.clip),
      transform_id(other.transform_id) {
  if (other.clip_expander) {
    DCHECK_EQ(clip_type, ClipType::EXPANDS_CLIP);
    clip_expander = base::MakeUnique<ClipExpander>(*other.clip_expander);
  }
  cached_clip_rects = other.cached_clip_rects;
  cached_accumulated_rect_in_screen_space =
      other.cached_accumulated_rect_in_screen_space;
}

ClipNode& ClipNode::operator=(const ClipNode& other) {
  id = other.id;
  parent_id = other.parent_id;
  clip_type = other.clip_type;
  clip = other.clip;
  transform_id = other.transform_id;

  if (other.clip_expander) {
    DCHECK_EQ(clip_type, ClipType::EXPANDS_CLIP);
    clip_expander = base::MakeUnique<ClipExpander>(*other.clip_expander);
  } else {
    clip_expander.reset();
  }
  cached_clip_rects = other.cached_clip_rects;
  cached_accumulated_rect_in_screen_space =
      other.cached_accumulated_rect_in_screen_space;
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
         clip_type == other.clip_type && clip == other.clip &&
         transform_id == other.transform_id;
}

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("clip_type", static_cast<int>(clip_type));
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
}

}  // namespace cc

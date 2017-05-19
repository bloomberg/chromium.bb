// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_EFFECT_NODE_H_
#define CC_TREES_EFFECT_NODE_H_

#include "cc/base/filter_operations.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT EffectNode {
  EffectNode();
  EffectNode(const EffectNode& other);

  // The node index of this node in the effect tree node vector.
  int id;
  // The node index of the parent node in the effect tree node vector.
  int parent_id;
  // The layer id of the layer that owns this node.
  int owning_layer_id;

  float opacity;
  float screen_space_opacity;

  FilterOperations filters;
  FilterOperations background_filters;
  gfx::PointF filters_origin;

  SkBlendMode blend_mode;

  gfx::Vector2dF surface_contents_scale;

  gfx::Size unscaled_mask_target_size;

  bool has_render_surface : 1;
  bool has_copy_request : 1;
  bool hidden_by_backface_visibility : 1;
  bool double_sided : 1;
  bool is_drawn : 1;
  // TODO(jaydasika) : Delete this after implementation of
  // SetHideLayerAndSubtree is cleaned up. (crbug.com/595843)
  bool subtree_hidden : 1;
  bool has_potential_filter_animation : 1;
  bool has_potential_opacity_animation : 1;
  bool is_currently_animating_filter : 1;
  bool is_currently_animating_opacity : 1;
  // We need to track changes to effects on the compositor to compute damage
  // rect.
  bool effect_changed : 1;
  bool subtree_has_copy_request : 1;
  int transform_id;
  int clip_id;

  // This is the id of the ancestor effect node that induces a
  // RenderSurfaceImpl.
  int target_id;
  int mask_layer_id;
  int closest_ancestor_with_copy_request_id;

  bool operator==(const EffectNode& other) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_EFFECT_NODE_H_

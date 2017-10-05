// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRANSFORM_NODE_H_
#define CC_TREES_TRANSFORM_NODE_H_

#include "cc/cc_export.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT TransformNode {
  TransformNode();
  TransformNode(const TransformNode&);

  // ==== Input Variables ====
  // These values are generated from layer properties (in SPv1 mode),
  // or directly set by the client (in SPv2 mode).
  // Some of them may be mutated by the impl-thread, e.g., driven by animation,
  // scrolling, zooming, e.t.c..

  // The node index of this node in the transform tree node vector.
  int id = TransformTree::kInvalidNodeId;
  // The node index of the parent node in the transform tree node vector.
  int parent_id = TransformTree::kInvalidNodeId;

  ElementId element_id;

  // The local transform information is combined to form to_parent as follows:
  //
  //   to_parent = post_local_scale
  //       * translate(source_offset + transform_origin + source_to_parent
  //                 - scroll_offset + fixed_pos_offset + sticky_pos_offset)
  //       * local * translate(-transform_origin)
  float post_local_scale_factor = 1.0f;
  gfx::Vector2dF source_offset;
  gfx::Vector2dF source_to_parent;
  gfx::ScrollOffset scroll_offset;
  gfx::Vector3dF transform_origin;
  gfx::Transform local;

  // This is an index to the sticky position data array which defines the
  // sticky constraints for this transform node. -1 indicates there are no
  // sticky position constraints.
  int sticky_position_constraint_id = -1;

  // This is the node with respect to which source_offset is defined. This will
  // not be needed once layerization moves to cc, but is needed in order to
  // efficiently update the transform tree for changes to position in the layer
  // tree.
  int source_node_id = TransformTree::kInvalidNodeId;

  // This id determines which 3d rendering context the node is in. 0 is a
  // special value and indicates that the node is not in any 3d rendering
  // context.
  int sorting_context_id = 0;

  // Flattening, when needed, is only applied to a node's inherited transform,
  // never to its local transform.
  bool flattens_inherited_transform : 1;

  bool scrolls : 1;

  bool should_be_snapped : 1;

  // These are used to position nodes wrt the right or bottom of the outer
  // viewport.
  bool moved_by_outer_viewport_bounds_delta_x : 1;
  bool moved_by_outer_viewport_bounds_delta_y : 1;

  // Layer scale factor is used as a fallback when we either cannot adjust
  // raster scale or if the raster scale cannot be extracted from the screen
  // space transform. For layers in the subtree of the page scale layer, the
  // layer scale factor should include the page scale factor.
  bool in_subtree_of_page_scale_layer : 1;
  // ==== End of Input Variables ====

  // ==== Derived Values and Volatile States ====
  // These values are derived from input variables above, or internal
  // bookkeeping states.
  // TODO(trchen): Move some of these to TransformCachedNodeData?

  // The combined matrix of all local transform components. See comment above.
  gfx::Transform to_parent;

  // Set to true if any component contributing to |to_parent| changed,
  // so |to_parent| will be recalculated.
  bool needs_local_transform_update : 1;

  // Whether this node or any ancestor has a potentially running
  // (i.e., irrespective of exact timeline) transform animation or an
  // invertible transform.
  bool node_and_ancestors_are_animated_or_invertible : 1;

  // = to_parent.IsInvertible()
  bool is_invertible : 1;
  // Whether the transform from this node to the screen is
  // invertible.
  bool ancestors_are_invertible : 1;

  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) transform animation.
  bool has_potential_animation : 1;
  // Whether this node has a currently running transform animation.
  bool is_currently_animating : 1;
  // Whether this node *or an ancestor* has a potentially running
  // (i.e., irrespective of exact timeline) transform
  // animation.
  bool to_screen_is_potentially_animated : 1;
  // Whether all animations on this transform node are simple
  // translations.
  bool has_only_translation_animations : 1;

  // This is true if the to_parent transform at every node on the path to the
  // root is flat.
  bool node_and_ancestors_are_flat : 1;

  // This is needed to know if a layer can use lcd text.
  bool node_and_ancestors_have_only_integer_translation : 1;

  // We need to track changes to to_screen transform to compute the damage rect.
  bool transform_changed : 1;

  // This value stores the snapped amount whenever we snap. If the snap is due
  // to a scroll, we need it to calculate fixed-pos elements adjustment, even
  // otherwise we may need it to undo the snapping next frame.
  gfx::Vector2dF snap_amount;
  // ==== End of Derived Values and Volatile States ====

  bool operator==(const TransformNode& other) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

// TODO(sunxd): move this into PropertyTrees::cached_data_.
struct CC_EXPORT TransformCachedNodeData {
  TransformCachedNodeData();
  TransformCachedNodeData(const TransformCachedNodeData& other);
  ~TransformCachedNodeData();

  gfx::Transform from_screen;
  gfx::Transform to_screen;

  bool is_showing_backface : 1;

  bool operator==(const TransformCachedNodeData& other) const;
};

}  // namespace cc

#endif  // CC_TREES_TRANSFORM_NODE_H_

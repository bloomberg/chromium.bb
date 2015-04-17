// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include <map>
#include <set>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

class LayerTreeHost;

namespace {

struct DataForRecursion {
  TransformTree* transform_tree;
  ClipTree* clip_tree;
  OpacityTree* opacity_tree;
  Layer* transform_tree_parent;
  Layer* transform_fixed_parent;
  Layer* render_target;
  int clip_tree_parent;
  int opacity_tree_parent;
  const Layer* page_scale_layer;
  float page_scale_factor;
  float device_scale_factor;
  bool in_subtree_of_page_scale_application_layer;
  bool should_flatten;
  bool ancestor_clips_subtree;
  const gfx::Transform* device_transform;
  gfx::Vector2dF scroll_compensation_adjustment;
};

static Layer* GetTransformParent(const DataForRecursion& data, Layer* layer) {
  return layer->position_constraint().is_fixed_position()
             ? data.transform_fixed_parent
             : data.transform_tree_parent;
}

static ClipNode* GetClipParent(const DataForRecursion& data, Layer* layer) {
  const bool inherits_clip = !layer->parent() || !layer->clip_parent();
  const int id = inherits_clip ? data.clip_tree_parent
                               : layer->clip_parent()->clip_tree_index();
  return data.clip_tree->Node(id);
}

static bool HasPotentiallyRunningAnimation(Layer* layer,
                                           Animation::TargetProperty property) {
  if (Animation* animation =
          layer->layer_animation_controller()->GetAnimation(property)) {
    return !animation->is_finished();
  }
  return false;
}

static bool RequiresClipNode(Layer* layer,
                             const DataForRecursion& data,
                             int parent_transform_id,
                             bool is_clipped) {
  const bool render_surface_applies_clip =
      layer->render_surface() && is_clipped;
  const bool render_surface_may_grow_due_to_clip_children =
      layer->render_surface() && layer->num_unclipped_descendants() > 0;

  if (!layer->parent() || layer->masks_to_bounds() || layer->mask_layer() ||
      render_surface_may_grow_due_to_clip_children)
    return true;

  if (!render_surface_applies_clip)
    return false;

  bool axis_aligned_with_respect_to_parent =
      data.transform_tree->Are2DAxisAligned(layer->transform_tree_index(),
                                            parent_transform_id);

  return !axis_aligned_with_respect_to_parent;
}

static bool LayerClipsSubtree(Layer* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

void AddClipNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                         Layer* layer,
                         bool created_transform_node,
                         DataForRecursion* data_for_children) {
  ClipNode* parent = GetClipParent(data_from_ancestor, layer);
  int parent_id = parent->id;

  bool ancestor_clips_subtree =
      data_from_ancestor.ancestor_clips_subtree || layer->clip_parent();

  data_for_children->ancestor_clips_subtree = false;
  bool has_unclipped_surface = false;

  if (layer->has_render_surface()) {
    if (ancestor_clips_subtree && layer->num_unclipped_descendants() > 0)
      data_for_children->ancestor_clips_subtree = true;
    else if (!ancestor_clips_subtree && !layer->num_unclipped_descendants())
      has_unclipped_surface = true;
  } else {
    data_for_children->ancestor_clips_subtree = ancestor_clips_subtree;
  }

  if (LayerClipsSubtree(layer))
    data_for_children->ancestor_clips_subtree = true;

  if (has_unclipped_surface)
    parent_id = 0;

  if (!RequiresClipNode(layer, data_from_ancestor, parent->data.transform_id,
                        data_for_children->ancestor_clips_subtree)) {
    // Unclipped surfaces reset the clip rect.
    data_for_children->clip_tree_parent = parent_id;
  } else if (layer->parent()) {
    // Note the root clip gets handled elsewhere.
    Layer* transform_parent = data_for_children->transform_tree_parent;
    if (layer->position_constraint().is_fixed_position() &&
        !created_transform_node) {
      transform_parent = data_for_children->transform_fixed_parent;
    }
    ClipNode node;
    node.data.clip = gfx::RectF(
        gfx::PointF() + layer->offset_to_transform_parent(), layer->bounds());
    node.data.transform_id = transform_parent->transform_tree_index();
    node.data.target_id =
        data_for_children->render_target->transform_tree_index();
    node.owner_id = layer->id();

    data_for_children->clip_tree_parent =
        data_for_children->clip_tree->Insert(node, parent_id);
  }

  layer->set_clip_tree_index(
      has_unclipped_surface ? 0 : data_for_children->clip_tree_parent);

  // TODO(awoloszyn): Right now when we hit a node with a replica, we reset the
  // clip for all children since we may need to draw. We need to figure out a
  // better way, since we will need both the clipped and unclipped versions.
}

bool AddTransformNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                              Layer* layer,
                              DataForRecursion* data_for_children) {
  const bool is_root = !layer->parent();
  const bool is_page_scale_application_layer =
      layer->parent() && layer->parent() == data_from_ancestor.page_scale_layer;
  const bool is_scrollable = layer->scrollable();
  const bool is_fixed = layer->position_constraint().is_fixed_position();

  const bool has_significant_transform =
      !layer->transform().IsIdentityOr2DTranslation();

  const bool has_potentially_animated_transform =
      HasPotentiallyRunningAnimation(layer, Animation::TRANSFORM);

  const bool has_animated_transform =
      layer->layer_animation_controller()->IsAnimatingProperty(
          Animation::TRANSFORM);

  const bool has_surface = !!layer->render_surface();

  bool requires_node = is_root || is_scrollable || has_significant_transform ||
                       has_potentially_animated_transform || has_surface ||
                       is_page_scale_application_layer;

  Layer* transform_parent = GetTransformParent(data_from_ancestor, layer);

  gfx::Vector2dF parent_offset;
  if (transform_parent) {
    if (layer->scroll_parent()) {
      gfx::Transform to_parent;
      Layer* source = layer->parent();
      parent_offset += source->offset_to_transform_parent();
      data_from_ancestor.transform_tree->ComputeTransform(
          source->transform_tree_index(),
          transform_parent->transform_tree_index(), &to_parent);
      parent_offset += to_parent.To2dTranslation();
    } else if (!is_fixed) {
      parent_offset = transform_parent->offset_to_transform_parent();
    } else {
      if (data_from_ancestor.transform_tree_parent !=
          data_from_ancestor.transform_fixed_parent) {
        gfx::Vector2dF fixed_offset = data_from_ancestor.transform_tree_parent
                                          ->offset_to_transform_parent();
        gfx::Transform parent_to_parent;
        data_from_ancestor.transform_tree->ComputeTransform(
            data_from_ancestor.transform_tree_parent->transform_tree_index(),
            data_from_ancestor.transform_fixed_parent->transform_tree_index(),
            &parent_to_parent);

        fixed_offset += parent_to_parent.To2dTranslation();
        parent_offset += fixed_offset;
      }
      parent_offset += data_from_ancestor.scroll_compensation_adjustment;
    }
  }

  if (layer->IsContainerForFixedPositionLayers() || is_root)
    data_for_children->transform_fixed_parent = layer;
  data_for_children->transform_tree_parent = layer;

  if (layer->IsContainerForFixedPositionLayers() || is_fixed)
    data_for_children->scroll_compensation_adjustment = gfx::Vector2dF();

  if (!requires_node) {
    data_for_children->should_flatten |= layer->should_flatten_transform();
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  layer->transform().To2dTranslation();
    layer->set_offset_to_transform_parent(parent_offset + local_offset);
    layer->set_should_flatten_transform_from_property_tree(
        data_from_ancestor.should_flatten);
    layer->set_transform_tree_index(transform_parent->transform_tree_index());
    return false;
  }

  int parent_index = 0;
  if (transform_parent)
    parent_index = transform_parent->transform_tree_index();

  data_for_children->transform_tree->Insert(TransformNode(), parent_index);

  TransformNode* node = data_for_children->transform_tree->back();
  layer->set_transform_tree_index(node->id);

  node->data.scrolls = is_scrollable;
  node->data.flattens_inherited_transform = data_for_children->should_flatten;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      layer->should_flatten_transform() || has_surface;
  node->data.target_id =
      data_from_ancestor.render_target->transform_tree_index();
  node->data.content_target_id =
      data_for_children->render_target->transform_tree_index();
  DCHECK_NE(node->data.target_id, -1);
  node->data.is_animated = has_animated_transform;

  float post_local_scale_factor = 1.0f;
  if (is_root) {
    node->data.post_local = *data_from_ancestor.device_transform;
    post_local_scale_factor = data_from_ancestor.device_scale_factor;
  }

  if (is_page_scale_application_layer)
    post_local_scale_factor *= data_from_ancestor.page_scale_factor;

  if (has_surface && !is_root) {
    node->data.needs_sublayer_scale = true;
    node->data.layer_scale_factor = data_from_ancestor.device_scale_factor;
    if (data_from_ancestor.in_subtree_of_page_scale_application_layer)
      node->data.layer_scale_factor *= data_from_ancestor.page_scale_factor;
  }

  if (is_root) {
    node->data.post_local.Scale(post_local_scale_factor,
                                post_local_scale_factor);
  } else {
    node->data.post_local_scale_factor = post_local_scale_factor;
    node->data.parent_offset = parent_offset;
    node->data.update_post_local_transform(layer->position(),
                                           layer->transform_origin());
  }

  if (!layer->scroll_parent()) {
    node->data.scroll_offset =
        gfx::ScrollOffsetToVector2dF(layer->CurrentScrollOffset());
  }

  node->data.local = layer->transform();
  node->data.pre_local.Translate3d(-layer->transform_origin().x(),
                                   -layer->transform_origin().y(),
                                   -layer->transform_origin().z());

  node->data.needs_local_transform_update = true;
  data_from_ancestor.transform_tree->UpdateTransforms(node->id);

  layer->set_offset_to_transform_parent(gfx::Vector2dF());

  // Flattening (if needed) will be handled by |node|.
  layer->set_should_flatten_transform_from_property_tree(false);

  data_for_children->scroll_compensation_adjustment +=
      layer->ScrollCompensationAdjustment() - node->data.scroll_snap;

  node->owner_id = layer->id();

  return true;
}

void AddOpacityNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                            Layer* layer,
                            DataForRecursion* data_for_children) {
  const bool is_root = !layer->parent();
  const bool has_transparency = layer->opacity() != 1.f;
  const bool has_animated_opacity =
      HasPotentiallyRunningAnimation(layer, Animation::OPACITY) ||
      layer->OpacityCanAnimateOnImplThread();
  bool requires_node = is_root || has_transparency || has_animated_opacity;

  int parent_id = data_from_ancestor.opacity_tree_parent;

  if (!requires_node) {
    layer->set_opacity_tree_index(parent_id);
    data_for_children->opacity_tree_parent = parent_id;
    return;
  }

  OpacityNode node;
  node.owner_id = layer->id();
  node.data = layer->opacity();
  data_for_children->opacity_tree_parent =
      data_for_children->opacity_tree->Insert(node, parent_id);
  layer->set_opacity_tree_index(data_for_children->opacity_tree_parent);
}

void BuildPropertyTreesInternal(Layer* layer,
                                const DataForRecursion& data_from_parent) {
  DataForRecursion data_for_children(data_from_parent);
  if (layer->render_surface())
    data_for_children.render_target = layer;

  bool created_transform_node =
      AddTransformNodeIfNeeded(data_from_parent, layer, &data_for_children);
  AddClipNodeIfNeeded(data_from_parent, layer, created_transform_node,
                      &data_for_children);

  if (data_from_parent.opacity_tree)
    AddOpacityNodeIfNeeded(data_from_parent, layer, &data_for_children);

  if (layer == data_from_parent.page_scale_layer)
    data_for_children.in_subtree_of_page_scale_application_layer = true;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    if (!layer->children()[i]->scroll_parent())
      BuildPropertyTreesInternal(layer->children()[i].get(), data_for_children);
  }

  if (layer->scroll_children()) {
    for (Layer* scroll_child : *layer->scroll_children()) {
      BuildPropertyTreesInternal(scroll_child, data_for_children);
    }
  }

  if (layer->has_replica())
    BuildPropertyTreesInternal(layer->replica_layer(), data_for_children);
}

}  // namespace

void PropertyTreeBuilder::BuildPropertyTrees(
    Layer* root_layer,
    const Layer* page_scale_layer,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  if (!property_trees->needs_rebuild)
    return;

  DataForRecursion data_for_recursion;
  data_for_recursion.transform_tree = &property_trees->transform_tree;
  data_for_recursion.clip_tree = &property_trees->clip_tree;
  data_for_recursion.opacity_tree = &property_trees->opacity_tree;
  data_for_recursion.transform_tree_parent = nullptr;
  data_for_recursion.transform_fixed_parent = nullptr;
  data_for_recursion.render_target = root_layer;
  data_for_recursion.clip_tree_parent = 0;
  data_for_recursion.opacity_tree_parent = -1;
  data_for_recursion.page_scale_layer = page_scale_layer;
  data_for_recursion.page_scale_factor = page_scale_factor;
  data_for_recursion.device_scale_factor = device_scale_factor;
  data_for_recursion.in_subtree_of_page_scale_application_layer = false;
  data_for_recursion.should_flatten = false;
  data_for_recursion.ancestor_clips_subtree = true;
  data_for_recursion.device_transform = &device_transform;

  data_for_recursion.transform_tree->clear();
  data_for_recursion.clip_tree->clear();
  data_for_recursion.opacity_tree->clear();

  ClipNode root_clip;
  root_clip.data.clip = viewport;
  root_clip.data.transform_id = 0;
  data_for_recursion.clip_tree_parent =
      data_for_recursion.clip_tree->Insert(root_clip, 0);
  BuildPropertyTreesInternal(root_layer, data_for_recursion);
  property_trees->needs_rebuild = false;
}

}  // namespace cc

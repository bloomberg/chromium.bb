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

namespace cc {

class LayerTreeHost;

namespace {

struct DataForRecursion {
  TransformTree* transform_tree;
  ClipTree* clip_tree;
  Layer* transform_tree_parent;
  Layer* transform_fixed_parent;
  Layer* render_target;
  int clip_tree_parent;
  gfx::Vector2dF offset_to_transform_tree_parent;
  gfx::Vector2dF offset_to_transform_fixed_parent;
  const Layer* page_scale_layer;
  float page_scale_factor;
  float device_scale_factor;
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

static bool RequiresClipNode(Layer* layer,
                             bool axis_aligned_with_respect_to_parent) {
  const bool render_surface_applies_non_axis_aligned_clip =
      layer->render_surface() && !axis_aligned_with_respect_to_parent &&
      layer->is_clipped();
  const bool render_surface_may_grow_due_to_clip_children =
      layer->render_surface() && layer->num_unclipped_descendants() > 0;

  return !layer->parent() || layer->masks_to_bounds() || layer->mask_layer() ||
         render_surface_applies_non_axis_aligned_clip ||
         render_surface_may_grow_due_to_clip_children;
}

void AddClipNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                         Layer* layer,
                         DataForRecursion* data_for_children) {
  ClipNode* parent = GetClipParent(data_from_ancestor, layer);
  int parent_id = parent->id;
  const bool axis_aligned_with_respect_to_parent =
      data_from_ancestor.transform_tree->Are2DAxisAligned(
          layer->transform_tree_index(), parent->data.transform_id);

  // TODO(vollick): once Andrew refactors the surface determinations out of
  // CDP, the the layer->render_surface() check will be invalid.
  const bool has_unclipped_surface =
      layer->render_surface() &&
      !layer->render_surface()->is_clipped() &&
      layer->num_unclipped_descendants() == 0;

  if (has_unclipped_surface)
    parent_id = 0;

  if (!RequiresClipNode(layer, axis_aligned_with_respect_to_parent)) {
    // Unclipped surfaces reset the clip rect.
    data_for_children->clip_tree_parent = parent_id;
  } else if (layer->parent()) {
    // Note the root clip gets handled elsewhere.
    Layer* transform_parent = GetTransformParent(*data_for_children, layer);
    ClipNode node;
    node.data.clip = gfx::RectF(
        gfx::PointF() + layer->offset_to_transform_parent(), layer->bounds());
    node.data.transform_id = transform_parent->transform_tree_index();
    node.data.target_id =
        data_from_ancestor.render_target->transform_tree_index();

    data_for_children->clip_tree_parent =
        data_for_children->clip_tree->Insert(node, parent_id);
  }

  layer->set_clip_tree_index(
      has_unclipped_surface ? 0 : data_for_children->clip_tree_parent);

  // TODO(awoloszyn): Right now when we hit a node with a replica, we reset the
  // clip for all children since we may need to draw. We need to figure out a
  // better way, since we will need both the clipped and unclipped versions.
}

void AddTransformNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                              Layer* layer,
                              DataForRecursion* data_for_children) {
  const bool is_root = !layer->parent();
  const bool is_page_scale_application_layer =
      layer->parent() && layer->parent() == data_from_ancestor.page_scale_layer;
  const bool is_scrollable = layer->scrollable();
  const bool is_fixed = layer->position_constraint().is_fixed_position();

  const bool has_significant_transform =
      !layer->transform().IsIdentityOr2DTranslation();

  const bool has_animated_transform =
      layer->layer_animation_controller()->IsAnimatingProperty(
          Animation::Transform);

  const bool has_transform_origin = layer->transform_origin() != gfx::Point3F();

  const bool has_surface = !!layer->render_surface();

  const bool flattening_change = layer->parent() &&
                                 layer->should_flatten_transform() &&
                                 !layer->parent()->should_flatten_transform();

  bool requires_node = is_root || is_scrollable || is_fixed ||
                       has_significant_transform || has_animated_transform ||
                       is_page_scale_application_layer || flattening_change ||
                       has_transform_origin || has_surface;

  Layer* transform_parent = GetTransformParent(data_from_ancestor, layer);

  // May be non-zero if layer is fixed or has a scroll parent.
  gfx::Vector2dF parent_offset;
  if (transform_parent) {
    // TODO(vollick): This is to mimic existing bugs (crbug.com/441447).
    if (!is_fixed)
      parent_offset = transform_parent->offset_to_transform_parent();

    gfx::Transform to_parent;
    Layer* source = data_from_ancestor.transform_tree_parent;
    if (layer->scroll_parent()) {
      source = layer->parent();
      parent_offset += layer->parent()->offset_to_transform_parent();
    }
    data_from_ancestor.transform_tree->ComputeTransform(
        source->transform_tree_index(),
        transform_parent->transform_tree_index(), &to_parent);

    parent_offset += to_parent.To2dTranslation();
  }

  if (layer->IsContainerForFixedPositionLayers() || is_root)
    data_for_children->transform_fixed_parent = layer;
  data_for_children->transform_tree_parent = layer;

  if (!requires_node) {
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  layer->transform().To2dTranslation();
    layer->set_offset_to_transform_parent(parent_offset + local_offset);
    layer->set_transform_tree_index(transform_parent->transform_tree_index());
    return;
  }

  if (!is_root) {
    data_for_children->transform_tree->Insert(
        TransformNode(), transform_parent->transform_tree_index());
  }

  TransformNode* node = data_for_children->transform_tree->back();

  node->data.flattens = layer->should_flatten_transform();
  node->data.target_id =
      data_from_ancestor.render_target->transform_tree_index();
  node->data.is_animated = layer->TransformIsAnimating();

  gfx::Transform transform;
  float device_and_page_scale_factors = 1.0f;
  if (is_root)
    device_and_page_scale_factors = data_from_ancestor.device_scale_factor;
  if (is_page_scale_application_layer)
    device_and_page_scale_factors *= data_from_ancestor.page_scale_factor;

  transform.Scale(device_and_page_scale_factors, device_and_page_scale_factors);

  // TODO(vollick): We've accounted for the scroll offset here but we haven't
  // taken into account snapping to screen space pixels. For the purposes of
  // computing rects we need to record, this should be fine (the visible rects
  // we compute may be slightly different than what we'd compute with snapping,
  // but since we significantly expand the visible rect when determining what to
  // record, the slight difference should be inconsequential).
  gfx::Vector2dF position = layer->position().OffsetFromOrigin();
  if (!layer->scroll_parent()) {
    position -= gfx::Vector2dF(layer->TotalScrollOffset().x(),
        layer->TotalScrollOffset().y());
  }

  position += parent_offset;

  transform.Translate3d(position.x() + layer->transform_origin().x(),
                        position.y() + layer->transform_origin().y(),
                        layer->transform_origin().z());
  transform.PreconcatTransform(layer->transform());
  transform.Translate3d(-layer->transform_origin().x(),
                        -layer->transform_origin().y(),
                        -layer->transform_origin().z());

  node->data.to_parent = transform;
  node->data.is_invertible = transform.GetInverse(&node->data.from_parent);

  data_from_ancestor.transform_tree->UpdateScreenSpaceTransform(node->id);

  layer->set_offset_to_transform_parent(gfx::Vector2dF());
  layer->set_transform_tree_index(node->id);
}

void BuildPropertyTreesInternal(Layer* layer,
                                const DataForRecursion& data_from_parent) {
  DataForRecursion data_for_children(data_from_parent);
  if (layer->render_surface())
    data_for_children.render_target = layer;

  AddTransformNodeIfNeeded(data_from_parent, layer, &data_for_children);
  AddClipNodeIfNeeded(data_from_parent, layer, &data_for_children);

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
    TransformTree* transform_tree,
    ClipTree* clip_tree) {
  DataForRecursion data_for_recursion;
  data_for_recursion.transform_tree = transform_tree;
  data_for_recursion.clip_tree = clip_tree;
  data_for_recursion.transform_tree_parent = nullptr;
  data_for_recursion.transform_fixed_parent = nullptr;
  data_for_recursion.render_target = root_layer;
  data_for_recursion.clip_tree_parent = 0;
  data_for_recursion.page_scale_layer = page_scale_layer;
  data_for_recursion.page_scale_factor = page_scale_factor;
  data_for_recursion.device_scale_factor = device_scale_factor;

  int transform_root_id = transform_tree->Insert(TransformNode(), 0);

  ClipNode root_clip;
  root_clip.data.clip = viewport;
  root_clip.data.transform_id = 0;
  data_for_recursion.clip_tree_parent = clip_tree->Insert(root_clip, 0);

  BuildPropertyTreesInternal(root_layer, data_for_recursion);

  TransformNode* transform_root = transform_tree->Node(transform_root_id);
  transform_root->data.set_to_parent(device_transform *
                                     transform_root->data.to_parent);
}

}  // namespace cc

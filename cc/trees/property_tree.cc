// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/logging.h"
#include "cc/base/math_util.h"
#include "cc/trees/property_tree.h"

namespace cc {

template <typename T>
PropertyTree<T>::PropertyTree() {
  nodes_.push_back(T());
  back()->id = 0;
  back()->parent_id = -1;
}

template <typename T>
PropertyTree<T>::~PropertyTree() {
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

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<OpacityNode>;

TransformNodeData::TransformNodeData()
    : target_id(-1),
      content_target_id(-1),
      needs_local_transform_update(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      is_animated(false),
      to_screen_is_animated(false),
      flattens_inherited_transform(false),
      flattens_local_transform(false),
      scrolls(false),
      needs_sublayer_scale(false),
      layer_scale_factor(1.0f) {
}

TransformNodeData::~TransformNodeData() {
}

ClipNodeData::ClipNodeData() : transform_id(-1), target_id(-1) {
}

bool TransformTree::ComputeTransform(int source_id,
                                     int dest_id,
                                     gfx::Transform* transform) const {
  transform->MakeIdentity();

  if (source_id == dest_id)
    return true;

  if (source_id > dest_id && IsDescendant(source_id, dest_id))
    return CombineTransformsBetween(source_id, dest_id, transform);

  if (dest_id > source_id && IsDescendant(dest_id, source_id))
    return CombineInversesBetween(source_id, dest_id, transform);

  int lca = LowestCommonAncestor(source_id, dest_id);

  bool no_singular_matrices_to_lca =
      CombineTransformsBetween(source_id, lca, transform);

  bool no_singular_matrices_from_lca =
      CombineInversesBetween(lca, dest_id, transform);

  return no_singular_matrices_to_lca && no_singular_matrices_from_lca;
}

bool TransformTree::Are2DAxisAligned(int source_id, int dest_id) const {
  gfx::Transform transform;
  return ComputeTransform(source_id, dest_id, &transform) &&
         transform.Preserves2dAxisAlignment();
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  TransformNode* target_node = Node(node->data.target_id);
  if (node->data.needs_local_transform_update)
    UpdateLocalTransform(node);
  UpdateScreenSpaceTransform(node, parent_node, target_node);
  UpdateSublayerScale(node);
  UpdateTargetSpaceTransform(node, target_node);
  UpdateIsAnimated(node, parent_node);
  UpdateSnapping(node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id < 0)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

int TransformTree::LowestCommonAncestor(int a, int b) const {
  std::set<int> chain_a;
  std::set<int> chain_b;
  while (a || b) {
    if (a) {
      a = Node(a)->parent_id;
      if (a > -1 && chain_b.find(a) != chain_b.end())
        return a;
      chain_a.insert(a);
    }
    if (b) {
      b = Node(b)->parent_id;
      if (b > -1 && chain_a.find(b) != chain_a.end())
        return b;
      chain_b.insert(b);
    }
  }
  NOTREACHED();
  return 0;
}

bool TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
  const TransformNode* current = Node(source_id);
  const TransformNode* dest = Node(dest_id);
  if (!dest || dest->data.ancestors_are_invertible) {
    transform->ConcatTransform(current->data.to_screen);
    if (dest)
      transform->ConcatTransform(dest->data.from_screen);
    return true;
  }

  bool all_are_invertible = true;
  for (; current && current->id > dest_id; current = parent(current)) {
    transform->ConcatTransform(current->data.to_parent);
    if (!current->data.is_invertible)
      all_are_invertible = false;
  }

  return all_are_invertible;
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  if (current->data.ancestors_are_invertible) {
    transform->PreconcatTransform(current->data.from_screen);
    if (dest)
      transform->PreconcatTransform(dest->data.to_screen);
    return true;
  }

  bool all_are_invertible = true;
  for (; current && current->id > source_id; current = parent(current)) {
    transform->PreconcatTransform(current->data.from_parent);
    if (!current->data.is_invertible)
      all_are_invertible = false;
  }

  return all_are_invertible;
}

void TransformTree::UpdateLocalTransform(TransformNode* node) {
  gfx::Transform transform = node->data.post_local;
  transform.Translate(-node->data.scroll_offset.x(),
                      -node->data.scroll_offset.y());
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
  } else if (parent_node->data.flattens_local_transform ||
             node->data.flattens_inherited_transform) {
    // Flattening is tricky. Once a layer is drawn into its render target, it
    // cannot escape, so we only need to consider transforms between the layer
    // and its target when flattening (i.e., its draw transform). To compute the
    // screen space transform when flattening is involved we combine three
    // transforms, A * B * C, where A is the screen space transform of the
    // target, B is the flattened draw transform of the layer's parent, and C is
    // the local transform.
    node->data.to_screen = target_node->data.to_screen;
    gfx::Transform flattened;
    ComputeTransform(parent_node->id, target_node->id, &flattened);
    flattened.FlattenTo2d();
    node->data.to_screen.PreconcatTransform(flattened);
    node->data.to_screen.PreconcatTransform(node->data.to_parent);
    node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
  } else {
    node->data.to_screen = parent_node->data.to_screen;
    node->data.to_screen.PreconcatTransform(node->data.to_parent);
    node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
  }

  if (!node->data.to_screen.GetInverse(&node->data.from_screen))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateSublayerScale(TransformNode* node) {
  // The sublayer scale depends on the screen space transform, so update it too.
  node->data.sublayer_scale =
      node->data.needs_sublayer_scale
          ? MathUtil::ComputeTransform2dScaleComponents(
                node->data.to_screen, node->data.layer_scale_factor)
          : gfx::Vector2dF(1.0f, 1.0f);
}

void TransformTree::UpdateTargetSpaceTransform(TransformNode* node,
                                               TransformNode* target_node) {
  node->data.to_target.MakeIdentity();
  if (node->data.needs_sublayer_scale) {
    node->data.to_target.Scale(node->data.sublayer_scale.x(),
                               node->data.sublayer_scale.y());
  } else {
    const bool target_is_root_surface = target_node->id == 1;
    // In order to include the root transform for the root surface, we walk up
    // to the root of the transform tree in ComputeTransform.
    int target_id = target_is_root_surface ? 0 : target_node->id;
    if (target_node) {
      node->data.to_target.Scale(target_node->data.sublayer_scale.x(),
                                 target_node->data.sublayer_scale.y());
    }

    gfx::Transform unscaled_target_transform;
    ComputeTransform(node->id, target_id, &unscaled_target_transform);
    node->data.to_target.PreconcatTransform(unscaled_target_transform);
  }

  if (!node->data.to_target.GetInverse(&node->data.from_target))
    node->data.ancestors_are_invertible = false;
}

void TransformTree::UpdateIsAnimated(TransformNode* node,
                                     TransformNode* parent_node) {
  if (parent_node) {
    node->data.to_screen_is_animated =
        node->data.is_animated || parent_node->data.to_screen_is_animated;
  }
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->data.scrolls || node->data.to_screen_is_animated ||
      !node->data.to_target.IsScaleOrTranslation()) {
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
  gfx::Transform inverse_delta(gfx::Transform::kSkipInitialization);
  bool invertible_delta = delta.GetInverse(&inverse_delta);

  // The delta should be a translation, modulo floating point error, and should
  // therefore be invertible.
  DCHECK(invertible_delta);

  // Now that we have our scroll delta, we must apply it to each of our
  // combined, to/from matrices.
  node->data.to_parent.PreconcatTransform(delta);
  node->data.from_parent.ConcatTransform(inverse_delta);
  node->data.to_target.PreconcatTransform(delta);
  node->data.from_target.ConcatTransform(inverse_delta);
  node->data.to_screen.PreconcatTransform(delta);
  node->data.from_screen.ConcatTransform(inverse_delta);
}

}  // namespace cc

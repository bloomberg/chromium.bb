// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/logging.h"
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

TransformNodeData::TransformNodeData()
    : target_id(-1),
      is_invertible(true),
      ancestors_are_invertible(true),
      is_animated(false),
      to_screen_is_animated(false),
      flattens(false) {
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

void TransformTree::UpdateScreenSpaceTransform(int id) {
  TransformNode* current_node = Node(id);
  TransformNode* parent_node = parent(current_node);
  TransformNode* target_node = Node(current_node->data.target_id);

  if (!parent_node) {
    current_node->data.to_screen = current_node->data.to_parent;
    current_node->data.ancestors_are_invertible = true;
    current_node->data.to_screen_is_animated = false;
  } else if (parent_node->data.flattens) {
    // Flattening is tricky. Once a layer is drawn into its render target, it
    // cannot escape, so we only need to consider transforms between the layer
    // and its target when flattening (i.e., its draw transform). To compute the
    // screen space transform when flattening is involved we combine three
    // transforms, A * B * C, where A is the screen space transform of the
    // target, B is the flattened draw transform of the layer's parent, and C is
    // the local transform.
    current_node->data.to_screen = target_node->data.to_screen;
    gfx::Transform flattened;
    ComputeTransform(parent_node->id, target_node->id, &flattened);
    flattened.FlattenTo2d();
    current_node->data.to_screen.PreconcatTransform(flattened);
    current_node->data.to_screen.PreconcatTransform(
        current_node->data.to_parent);
    current_node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
  } else {
    current_node->data.to_screen = parent_node->data.to_screen;
    current_node->data.to_screen.PreconcatTransform(
        current_node->data.to_parent);
    current_node->data.ancestors_are_invertible =
        parent_node->data.ancestors_are_invertible;
  }
  if (!current_node->data.to_screen.GetInverse(&current_node->data.from_screen))
    current_node->data.ancestors_are_invertible = false;

  if (parent_node) {
    current_node->data.to_screen_is_animated =
        current_node->data.is_animated ||
        parent_node->data.to_screen_is_animated;
  }
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

}  // namespace cc

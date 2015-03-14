// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_H_
#define CC_TREES_PROPERTY_TREE_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

template <typename T>
struct CC_EXPORT TreeNode {
  TreeNode() : id(-1), parent_id(-1), data() {}
  int id;
  int parent_id;
  T data;
};

struct CC_EXPORT TransformNodeData {
  TransformNodeData();
  ~TransformNodeData();

  // The local transform information is combined to form to_parent (ignoring
  // snapping) as follows:
  //
  //   to_parent = M_post_local * T_scroll * M_local * M_pre_local.
  //
  // The pre/post may seem odd when read LTR, but we multiply our points from
  // the right, so the pre_local matrix affects the result "first". This lines
  // up with the notions of pre/post used in skia and gfx::Transform.
  //
  // TODO(vollick): The values labeled with "will be moved..." take up a lot of
  // space, but are only necessary for animated or scrolled nodes (otherwise
  // we'll just use the baked to_parent). These values will be ultimately stored
  // directly on the transform/scroll display list items when that's possible,
  // or potentially in a scroll tree.
  //
  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Transform pre_local;
  gfx::Transform local;
  gfx::Transform post_local;

  gfx::Transform to_parent;

  gfx::Transform to_target;
  gfx::Transform from_target;

  gfx::Transform to_screen;
  gfx::Transform from_screen;

  int target_id;
  // This id is used for all content that draws into a render surface associated
  // with this transform node.
  int content_target_id;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  bool needs_local_transform_update;

  bool is_invertible;
  bool ancestors_are_invertible;

  bool is_animated;
  bool to_screen_is_animated;

  // Flattening, when needed, is only applied to a node's inherited transform,
  // never to its local transform.
  bool flattens_inherited_transform;

  // This is true if the to_parent transform at every node on the path to the
  // root is flat.
  bool node_and_ancestors_are_flat;

  bool scrolls;

  bool needs_sublayer_scale;
  // This is used as a fallback when we either cannot adjust raster scale or if
  // the raster scale cannot be extracted from the screen space transform.
  float layer_scale_factor;
  gfx::Vector2dF sublayer_scale;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Vector2dF scroll_offset;

  void set_to_parent(const gfx::Transform& transform) {
    to_parent = transform;
    is_invertible = to_parent.IsInvertible();
  }
};

typedef TreeNode<TransformNodeData> TransformNode;

struct CC_EXPORT ClipNodeData {
  ClipNodeData();

  gfx::RectF clip;
  gfx::RectF combined_clip;
  int transform_id;
  int target_id;
};

typedef TreeNode<ClipNodeData> ClipNode;

typedef TreeNode<float> OpacityNode;

template <typename T>
class CC_EXPORT PropertyTree {
 public:
  PropertyTree();
  virtual ~PropertyTree();

  int Insert(const T& tree_node, int parent_id);

  T* Node(int i) { return i > -1 ? &nodes_[i] : nullptr; }
  const T* Node(int i) const { return i > -1 ? &nodes_[i] : nullptr; }

  T* parent(const T* t) {
    return t->parent_id > -1 ? Node(t->parent_id) : nullptr;
  }
  const T* parent(const T* t) const {
    return t->parent_id > -1 ? Node(t->parent_id) : nullptr;
  }

  T* back() { return size() ? &nodes_[nodes_.size() - 1] : nullptr; }
  const T* back() const {
    return size() ? &nodes_[nodes_.size() - 1] : nullptr;
  }

  void clear() { nodes_.clear(); }
  size_t size() const { return nodes_.size(); }

 private:
  // Copy and assign are permitted. This is how we do tree sync.
  std::vector<T> nodes_;
};

class CC_EXPORT TransformTree final : public PropertyTree<TransformNode> {
 public:
  // Computes the change of basis transform from node |source_id| to |dest_id|.
  // The function returns false iff the inverse of a singular transform was
  // used (and the result should, therefore, not be trusted).
  bool ComputeTransform(int source_id,
                        int dest_id,
                        gfx::Transform* transform) const;

  // Returns true iff the nodes indexed by |source_id| and |dest_id| are 2D axis
  // aligned with respect to one another.
  bool Are2DAxisAligned(int source_id, int dest_id) const;

  // Updates the parent, target, and screen space transforms and snapping.
  void UpdateTransforms(int id);

 private:
  // Returns true iff the node at |desc_id| is a descendant of the node at
  // |anc_id|.
  bool IsDescendant(int desc_id, int anc_id) const;

  // Returns the index of the lowest common ancestor of the nodes |a| and |b|.
  int LowestCommonAncestor(int a, int b) const;

  // Computes the combined transform between |source_id| and |dest_id| and
  // returns false if the inverse of a singular transform was used. These two
  // nodes must be on the same ancestor chain.
  bool CombineTransformsBetween(int source_id,
                                int dest_id,
                                gfx::Transform* transform) const;

  // Computes the combined inverse transform between |source_id| and |dest_id|
  // and returns false if the inverse of a singular transform was used. These
  // two nodes must be on the same ancestor chain.
  bool CombineInversesBetween(int source_id,
                              int dest_id,
                              gfx::Transform* transform) const;

  void UpdateLocalTransform(TransformNode* node);
  void UpdateScreenSpaceTransform(TransformNode* node,
                                  TransformNode* parent_node,
                                  TransformNode* target_node);
  void UpdateSublayerScale(TransformNode* node);
  void UpdateTargetSpaceTransform(TransformNode* node,
                                  TransformNode* target_node);
  void UpdateIsAnimated(TransformNode* node, TransformNode* parent_node);
  void UpdateSnapping(TransformNode* node);
};

class CC_EXPORT ClipTree final : public PropertyTree<ClipNode> {};

class CC_EXPORT OpacityTree final : public PropertyTree<OpacityNode> {};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_H_

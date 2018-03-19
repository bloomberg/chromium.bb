// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_RTREE_H_
#define CC_BASE_RTREE_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// The following description and most of the implementation is borrowed from
// Skia's SkRTree implementation.
//
// An R-Tree implementation. In short, it is a balanced n-ary tree containing a
// hierarchy of bounding rectangles.
//
// It only supports bulk-loading, i.e. creation from a batch of bounding
// rectangles. This performs a bottom-up bulk load using the STR
// (sort-tile-recursive) algorithm.
//
// Things to do: Experiment with other bulk-load algorithms (in particular the
// Hilbert pack variant, which groups rects by position on the Hilbert curve, is
// probably worth a look). There also exist top-down bulk load variants
// (VAMSplit, TopDownGreedy, etc).
//
// For more details see:
//
//  Beckmann, N.; Kriegel, H. P.; Schneider, R.; Seeger, B. (1990).
//  "The R*-tree: an efficient and robust access method for points and
//  rectangles"
template <typename T>
class RTree {
 public:
  RTree();
  ~RTree();

  // Constructs the rtree from a given container of gfx::Rects. Queries using
  // Search will then return indices into this container.
  template <typename Container>
  void Build(const Container& items);

  // Build helper that takes a container, a function used to get gfx::Rect
  // from each item, and a function used to get the payload for each item. That
  // is, "bounds_getter(items, i);" should return a gfx::Rect representing the
  // bounds of ith item, and "payload_getter(items, i);" should return the
  // payload (aka T) of ith item.
  template <typename Container, typename BoundsFunctor, typename PayloadFunctor>
  void Build(const Container& items,
             const BoundsFunctor& bounds_getter,
             const PayloadFunctor& payload_getter);

  // Given a query rect, returns elements that intersect the rect. Elements are
  // returned in the order they appeared in the initial container.
  std::vector<T> Search(const gfx::Rect& query) const;

  // Given a query rect, returns non-owning pointers to elements that intersect
  // the rect. Elements are returned in the order they appeared in the initial
  // container.
  std::vector<const T*> SearchRefs(const gfx::Rect& query) const;

  // Returns the total bounds of all items in this rtree.
  gfx::Rect GetBounds() const;

  // Returns respective bounds of all items in this rtree in the order of items.
  // Production code except tracing should not use this method.
  std::vector<gfx::Rect> GetAllBoundsForTracing() const;

  void Reset();

 private:
  // These values were empirically determined to produce reasonable performance
  // in most cases.
  enum { kMinChildren = 6 };
  enum { kMaxChildren = 11 };

  template <typename U>
  struct Node;

  template <typename U>
  struct Branch {
    // When the node level is 0, then the node is a leaf and the branch has a
    // valid index pointing to an element in the vector that was used to build
    // this rtree. When the level is not 0, it's an internal node and it has a
    // valid subtree pointer.
    Node<U>* subtree;
    U payload;

    gfx::Rect bounds;

    Branch() = default;
    Branch(U payload, const gfx::Rect& bounds)
        : payload(std::move(payload)), bounds(bounds) {}
  };

  template <typename U>
  struct Node {
    uint16_t num_children = 0u;
    uint16_t level = 0u;
    Branch<U> children[kMaxChildren];

    explicit Node(uint16_t level) : level(level) {}
  };

  void SearchRecursive(Node<T>* root,
                       const gfx::Rect& query,
                       std::vector<T>* results) const;
  void SearchRefsRecursive(Node<T>* root,
                           const gfx::Rect& query,
                           std::vector<const T*>* results) const;

  // Consumes the input array.
  Branch<T> BuildRecursive(std::vector<Branch<T>>* branches, int level);
  Node<T>* AllocateNodeAtLevel(int level);

  void GetAllBoundsRecursive(Node<T>* root,
                             std::vector<gfx::Rect>* results) const;

  // This is the count of data elements (rather than total nodes in the tree)
  size_t num_data_elements_ = 0u;
  Branch<T> root_;
  std::vector<Node<T>> nodes_;

  DISALLOW_COPY_AND_ASSIGN(RTree);
};

template <typename T>
RTree<T>::RTree() = default;

template <typename T>
RTree<T>::~RTree() = default;

template <typename T>
template <typename Container>
void RTree<T>::Build(const Container& items) {
  Build(items,
        [](const Container& items, size_t index) { return items[index]; },
        [](const Container& items, size_t index) { return index; });
}

template <typename T>
template <typename Container, typename BoundsFunctor, typename PayloadFunctor>
void RTree<T>::Build(const Container& items,
                     const BoundsFunctor& bounds_getter,
                     const PayloadFunctor& payload_getter) {
  DCHECK_EQ(0u, num_data_elements_);

  std::vector<Branch<T>> branches;
  branches.reserve(items.size());

  for (size_t i = 0; i < items.size(); i++) {
    const gfx::Rect& bounds = bounds_getter(items, i);
    if (bounds.IsEmpty())
      continue;
    branches.emplace_back(payload_getter(items, i), bounds);
  }

  num_data_elements_ = branches.size();
  if (num_data_elements_ == 1u) {
    nodes_.reserve(1);
    Node<T>* node = AllocateNodeAtLevel(0);
    root_.subtree = node;
    root_.bounds = branches[0].bounds;
    node->num_children = 1;
    node->children[0] = std::move(branches[0]);
  } else if (num_data_elements_ > 1u) {
    // Determine a reasonable upper bound on the number of nodes to prevent
    // reallocations. This is basically (n**d - 1) / (n - 1), which is the
    // number of nodes in a complete tree with n branches at each node. In the
    // code n = |branch_count|, d = |depth|. However, we normally would have
    // kMaxChildren branch factor, but that can be broken if some children
    // don't have enough nodes. That can happen for at most kMinChildren nodes
    // (since otherwise, we'd create a new node).
    size_t branch_count = kMaxChildren;
    double depth = log(branches.size()) / log(branch_count);
    size_t node_count =
        static_cast<size_t>((std::pow(branch_count, depth) - 1) /
                            (branch_count - 1)) +
        kMinChildren;
    nodes_.reserve(node_count);
    root_ = BuildRecursive(&branches, 0);
  }
  // We should've wasted at most kMinChildren nodes.
  DCHECK_LE(nodes_.capacity() - nodes_.size(),
            static_cast<size_t>(kMinChildren));
}

template <typename T>
auto RTree<T>::AllocateNodeAtLevel(int level) -> Node<T>* {
  // We don't allow reallocations, since that would invalidate references to
  // existing nodes, so verify that capacity > size.
  DCHECK_GT(nodes_.capacity(), nodes_.size());
  nodes_.emplace_back(level);
  return &nodes_.back();
}

template <typename T>
auto RTree<T>::BuildRecursive(std::vector<Branch<T>>* branches, int level)
    -> Branch<T> {
  // Only one branch.  It will be the root.
  if (branches->size() == 1)
    return std::move((*branches)[0]);

  // TODO(vmpstr): Investigate if branches should be sorted in y.
  // The comment from Skia reads:
  // We might sort our branches here, but we expect Blink gives us a reasonable
  // x,y order. Skipping a call to sort (in Y) here resulted in a 17% win for
  // recording with negligible difference in playback speed.
  int num_branches = static_cast<int>(branches->size() / kMaxChildren);
  int remainder = static_cast<int>(branches->size() % kMaxChildren);

  if (remainder > 0) {
    ++num_branches;
    // If the remainder isn't enough to fill a node, we'll add fewer nodes to
    // other branches.
    if (remainder >= kMinChildren)
      remainder = 0;
    else
      remainder = kMinChildren - remainder;
  }

  int num_strips = static_cast<int>(std::ceil(std::sqrt(num_branches)));
  int num_tiles = static_cast<int>(
      std::ceil(num_branches / static_cast<float>(num_strips)));
  size_t current_branch = 0;

  size_t new_branch_index = 0;
  for (int i = 0; i < num_strips; ++i) {
    // Might be worth sorting by X here too.
    for (int j = 0; j < num_tiles && current_branch < branches->size(); ++j) {
      int increment_by = kMaxChildren;
      if (remainder != 0) {
        // if need be, omit some nodes to make up for remainder
        if (remainder <= kMaxChildren - kMinChildren) {
          increment_by -= remainder;
          remainder = 0;
        } else {
          increment_by = kMinChildren;
          remainder -= kMaxChildren - kMinChildren;
        }
      }
      Node<T>* node = AllocateNodeAtLevel(level);
      node->num_children = 1;
      node->children[0] = (*branches)[current_branch];

      Branch<T> branch;
      branch.bounds = (*branches)[current_branch].bounds;
      branch.subtree = node;
      ++current_branch;
      int x = branch.bounds.x();
      int y = branch.bounds.y();
      int right = branch.bounds.right();
      int bottom = branch.bounds.bottom();
      for (int k = 1; k < increment_by && current_branch < branches->size();
           ++k) {
        // We use a custom union instead of gfx::Rect::Union here, since this
        // bypasses some empty checks and extra setters, which improves
        // performance.
        auto& bounds = (*branches)[current_branch].bounds;
        x = std::min(x, bounds.x());
        y = std::min(y, bounds.y());
        right = std::max(right, bounds.right());
        bottom = std::max(bottom, bounds.bottom());

        node->children[k] = (*branches)[current_branch];
        ++node->num_children;
        ++current_branch;
      }
      branch.bounds.SetRect(x, y, base::ClampSub(right, x),
                            base::ClampSub(bottom, y));

      DCHECK_LT(new_branch_index, current_branch);
      (*branches)[new_branch_index] = std::move(branch);
      ++new_branch_index;
    }
  }
  branches->resize(new_branch_index);
  return BuildRecursive(branches, level + 1);
}

template <typename T>
std::vector<T> RTree<T>::Search(const gfx::Rect& query) const {
  std::vector<T> results;
  if (num_data_elements_ > 0 && query.Intersects(root_.bounds))
    SearchRecursive(root_.subtree, query, &results);
  return results;
}

template <typename T>
std::vector<const T*> RTree<T>::SearchRefs(const gfx::Rect& query) const {
  std::vector<const T*> results;
  if (num_data_elements_ > 0 && query.Intersects(root_.bounds))
    SearchRefsRecursive(root_.subtree, query, &results);
  return results;
}

template <typename T>
void RTree<T>::SearchRecursive(Node<T>* node,
                               const gfx::Rect& query,
                               std::vector<T>* results) const {
  for (uint16_t i = 0; i < node->num_children; ++i) {
    if (query.Intersects(node->children[i].bounds)) {
      if (node->level == 0)
        results->push_back(node->children[i].payload);
      else
        SearchRecursive(node->children[i].subtree, query, results);
    }
  }
}

template <typename T>
void RTree<T>::SearchRefsRecursive(Node<T>* node,
                                   const gfx::Rect& query,
                                   std::vector<const T*>* results) const {
  for (uint16_t i = 0; i < node->num_children; ++i) {
    if (query.Intersects(node->children[i].bounds)) {
      if (node->level == 0)
        results->push_back(&node->children[i].payload);
      else
        SearchRefsRecursive(node->children[i].subtree, query, results);
    }
  }
}

template <typename T>
gfx::Rect RTree<T>::GetBounds() const {
  return root_.bounds;
}

template <typename T>
std::vector<gfx::Rect> RTree<T>::GetAllBoundsForTracing() const {
  std::vector<gfx::Rect> results;
  if (num_data_elements_ > 0)
    GetAllBoundsRecursive(root_.subtree, &results);
  return results;
}

template <typename T>
void RTree<T>::GetAllBoundsRecursive(Node<T>* node,
                                     std::vector<gfx::Rect>* results) const {
  for (uint16_t i = 0; i < node->num_children; ++i) {
    if (node->level == 0)
      results->push_back(node->children[i].bounds);
    else
      GetAllBoundsRecursive(node->children[i].subtree, results);
  }
}

template <typename T>
void RTree<T>::Reset() {
  num_data_elements_ = 0;
  nodes_.clear();
  root_.bounds = gfx::Rect();
}

}  // namespace cc

#endif  // CC_BASE_RTREE_H_

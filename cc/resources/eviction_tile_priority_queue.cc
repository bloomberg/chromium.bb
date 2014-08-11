// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/eviction_tile_priority_queue.h"

namespace cc {

namespace {

class EvictionOrderComparator {
 public:
  explicit EvictionOrderComparator(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}

  bool operator()(
      const EvictionTilePriorityQueue::PairedPictureLayerQueue* a,
      const EvictionTilePriorityQueue::PairedPictureLayerQueue* b) const {
    // Note that in this function, we have to return true if and only if
    // b is strictly lower priority than a. Note that for the sake of
    // completeness, empty queue is considered to have lowest priority.
    if (a->IsEmpty() || b->IsEmpty())
      return b->IsEmpty() < a->IsEmpty();

    WhichTree a_tree = a->NextTileIteratorTree(tree_priority_);
    const PictureLayerImpl::LayerEvictionTileIterator* a_iterator =
        a_tree == ACTIVE_TREE ? &a->active_iterator : &a->pending_iterator;

    WhichTree b_tree = b->NextTileIteratorTree(tree_priority_);
    const PictureLayerImpl::LayerEvictionTileIterator* b_iterator =
        b_tree == ACTIVE_TREE ? &b->active_iterator : &b->pending_iterator;

    const Tile* a_tile = **a_iterator;
    const Tile* b_tile = **b_iterator;

    const TilePriority& a_priority =
        a_tile->priority_for_tree_priority(tree_priority_);
    const TilePriority& b_priority =
        b_tile->priority_for_tree_priority(tree_priority_);
    bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

    // If the priority bin differs, b is lower priority if it has the higher
    // priority bin.
    if (a_priority.priority_bin != b_priority.priority_bin)
      return b_priority.priority_bin > a_priority.priority_bin;

    // Otherwise if the resolution differs, then the order will be determined by
    // whether we prioritize low res or not.
    // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
    // class but instead produced by the iterators.
    if (b_priority.resolution != a_priority.resolution) {
      // Non ideal resolution should be sorted higher than other resolutions.
      if (a_priority.resolution == NON_IDEAL_RESOLUTION)
        return false;

      if (b_priority.resolution == NON_IDEAL_RESOLUTION)
        return true;

      if (prioritize_low_res)
        return a_priority.resolution == LOW_RESOLUTION;
      return a_priority.resolution == HIGH_RESOLUTION;
    }

    // Otherwise if the occlusion differs, b is lower priority if it is
    // occluded.
    bool a_is_occluded = a_tile->is_occluded_for_tree_priority(tree_priority_);
    bool b_is_occluded = b_tile->is_occluded_for_tree_priority(tree_priority_);
    if (a_is_occluded != b_is_occluded)
      return b_is_occluded;

    // b is lower priorty if it is farther from visible.
    return b_priority.distance_to_visible > a_priority.distance_to_visible;
  }

 private:
  TreePriority tree_priority_;
};

}  // namespace

EvictionTilePriorityQueue::EvictionTilePriorityQueue() {
}

EvictionTilePriorityQueue::~EvictionTilePriorityQueue() {
}

void EvictionTilePriorityQueue::Build(
    const std::vector<PictureLayerImpl::Pair>& paired_layers,
    TreePriority tree_priority) {
  tree_priority_ = tree_priority;

  for (std::vector<PictureLayerImpl::Pair>::const_iterator it =
           paired_layers.begin();
       it != paired_layers.end();
       ++it) {
    paired_queues_.push_back(
        make_scoped_ptr(new PairedPictureLayerQueue(*it, tree_priority_)));
  }

  paired_queues_.make_heap(EvictionOrderComparator(tree_priority_));
}

void EvictionTilePriorityQueue::Reset() {
  paired_queues_.clear();
}

bool EvictionTilePriorityQueue::IsEmpty() const {
  return paired_queues_.empty() || paired_queues_.front()->IsEmpty();
}

Tile* EvictionTilePriorityQueue::Top() {
  DCHECK(!IsEmpty());
  return paired_queues_.front()->Top(tree_priority_);
}

void EvictionTilePriorityQueue::Pop() {
  DCHECK(!IsEmpty());

  paired_queues_.pop_heap(EvictionOrderComparator(tree_priority_));
  PairedPictureLayerQueue* paired_queue = paired_queues_.back();
  paired_queue->Pop(tree_priority_);
  paired_queues_.push_heap(EvictionOrderComparator(tree_priority_));
}

EvictionTilePriorityQueue::PairedPictureLayerQueue::PairedPictureLayerQueue() {
}

EvictionTilePriorityQueue::PairedPictureLayerQueue::PairedPictureLayerQueue(
    const PictureLayerImpl::Pair& layer_pair,
    TreePriority tree_priority)
    : active_iterator(
          layer_pair.active
              ? PictureLayerImpl::LayerEvictionTileIterator(layer_pair.active,
                                                            tree_priority)
              : PictureLayerImpl::LayerEvictionTileIterator()),
      pending_iterator(
          layer_pair.pending
              ? PictureLayerImpl::LayerEvictionTileIterator(layer_pair.pending,
                                                            tree_priority)
              : PictureLayerImpl::LayerEvictionTileIterator()) {
}

EvictionTilePriorityQueue::PairedPictureLayerQueue::~PairedPictureLayerQueue() {
}

bool EvictionTilePriorityQueue::PairedPictureLayerQueue::IsEmpty() const {
  return !active_iterator && !pending_iterator;
}

Tile* EvictionTilePriorityQueue::PairedPictureLayerQueue::Top(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  PictureLayerImpl::LayerEvictionTileIterator* next_iterator =
      next_tree == ACTIVE_TREE ? &active_iterator : &pending_iterator;
  DCHECK(*next_iterator);

  Tile* tile = **next_iterator;
  DCHECK(std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   tile) == returned_shared_tiles.end());
  return tile;
}

void EvictionTilePriorityQueue::PairedPictureLayerQueue::Pop(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  PictureLayerImpl::LayerEvictionTileIterator* next_iterator =
      next_tree == ACTIVE_TREE ? &active_iterator : &pending_iterator;
  DCHECK(*next_iterator);
  returned_shared_tiles.push_back(**next_iterator);
  ++(*next_iterator);

  if (IsEmpty())
    return;

  next_tree = NextTileIteratorTree(tree_priority);
  next_iterator =
      next_tree == ACTIVE_TREE ? &active_iterator : &pending_iterator;
  while (std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   **next_iterator) != returned_shared_tiles.end()) {
    ++(*next_iterator);
    if (IsEmpty())
      break;
    next_tree = NextTileIteratorTree(tree_priority);
    next_iterator =
        next_tree == ACTIVE_TREE ? &active_iterator : &pending_iterator;
  }
}

WhichTree
EvictionTilePriorityQueue::PairedPictureLayerQueue::NextTileIteratorTree(
    TreePriority tree_priority) const {
  DCHECK(!IsEmpty());

  // If we only have one iterator with tiles, return it.
  if (!active_iterator)
    return PENDING_TREE;
  if (!pending_iterator)
    return ACTIVE_TREE;

  const Tile* active_tile = *active_iterator;
  const Tile* pending_tile = *pending_iterator;
  if (active_tile == pending_tile)
    return ACTIVE_TREE;

  const TilePriority& active_priority =
      active_tile->priority_for_tree_priority(tree_priority);
  const TilePriority& pending_priority =
      pending_tile->priority_for_tree_priority(tree_priority);

  if (pending_priority.IsHigherPriorityThan(active_priority))
    return ACTIVE_TREE;
  return PENDING_TREE;
}

}  // namespace cc

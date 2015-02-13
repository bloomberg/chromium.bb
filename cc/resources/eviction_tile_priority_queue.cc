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
      const EvictionTilePriorityQueue::PairedTilingSetQueue* a,
      const EvictionTilePriorityQueue::PairedTilingSetQueue* b) const {
    // Note that in this function, we have to return true if and only if
    // b is strictly lower priority than a. Note that for the sake of
    // completeness, empty queue is considered to have lowest priority.
    if (a->IsEmpty() || b->IsEmpty())
      return b->IsEmpty() < a->IsEmpty();

    WhichTree a_tree = a->NextTileIteratorTree();
    const TilingSetEvictionQueue* a_queue =
        a_tree == ACTIVE_TREE ? a->active_queue.get() : a->pending_queue.get();

    WhichTree b_tree = b->NextTileIteratorTree();
    const TilingSetEvictionQueue* b_queue =
        b_tree == ACTIVE_TREE ? b->active_queue.get() : b->pending_queue.get();

    const Tile* a_tile = a_queue->Top();
    const Tile* b_tile = b_queue->Top();

    const TilePriority& a_priority = a_tile->combined_priority();
    const TilePriority& b_priority = b_tile->combined_priority();
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
    bool a_is_occluded = a_tile->is_occluded_combined();
    bool b_is_occluded = b_tile->is_occluded_combined();
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
        make_scoped_ptr(new PairedTilingSetQueue(*it, tree_priority_)));
  }

  paired_queues_.make_heap(EvictionOrderComparator(tree_priority_));
}

bool EvictionTilePriorityQueue::IsEmpty() const {
  return paired_queues_.empty() || paired_queues_.front()->IsEmpty();
}

Tile* EvictionTilePriorityQueue::Top() {
  DCHECK(!IsEmpty());
  return paired_queues_.front()->Top();
}

void EvictionTilePriorityQueue::Pop() {
  DCHECK(!IsEmpty());

  paired_queues_.pop_heap(EvictionOrderComparator(tree_priority_));
  PairedTilingSetQueue* paired_queue = paired_queues_.back();
  paired_queue->Pop();
  paired_queues_.push_heap(EvictionOrderComparator(tree_priority_));
}

EvictionTilePriorityQueue::PairedTilingSetQueue::PairedTilingSetQueue() {
}

EvictionTilePriorityQueue::PairedTilingSetQueue::PairedTilingSetQueue(
    const PictureLayerImpl::Pair& layer_pair,
    TreePriority tree_priority) {
  bool skip_shared_out_of_order_tiles = layer_pair.active && layer_pair.pending;
  if (layer_pair.active) {
    active_queue = make_scoped_ptr(new TilingSetEvictionQueue(
        layer_pair.active->picture_layer_tiling_set(), tree_priority,
        skip_shared_out_of_order_tiles));
  }
  if (layer_pair.pending) {
    pending_queue = make_scoped_ptr(new TilingSetEvictionQueue(
        layer_pair.pending->picture_layer_tiling_set(), tree_priority,
        skip_shared_out_of_order_tiles));
  }
}

EvictionTilePriorityQueue::PairedTilingSetQueue::~PairedTilingSetQueue() {
}

bool EvictionTilePriorityQueue::PairedTilingSetQueue::IsEmpty() const {
  return (!active_queue || active_queue->IsEmpty()) &&
         (!pending_queue || pending_queue->IsEmpty());
}

Tile* EvictionTilePriorityQueue::PairedTilingSetQueue::Top() {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree();
  TilingSetEvictionQueue* next_queue =
      next_tree == ACTIVE_TREE ? active_queue.get() : pending_queue.get();
  DCHECK(next_queue && !next_queue->IsEmpty());

  Tile* tile = next_queue->Top();
  DCHECK(returned_tiles_for_debug.find(tile) == returned_tiles_for_debug.end());
  return tile;
}

void EvictionTilePriorityQueue::PairedTilingSetQueue::Pop() {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree();
  TilingSetEvictionQueue* next_queue =
      next_tree == ACTIVE_TREE ? active_queue.get() : pending_queue.get();
  DCHECK(next_queue && !next_queue->IsEmpty());
  DCHECK(returned_tiles_for_debug.insert(next_queue->Top()).second);
  next_queue->Pop();

  // If not empty, use Top to DCHECK the next iterator.
  DCHECK_IMPLIES(!IsEmpty(), Top());
}

WhichTree
EvictionTilePriorityQueue::PairedTilingSetQueue::NextTileIteratorTree() const {
  DCHECK(!IsEmpty());

  // If we only have one iterator with tiles, return it.
  if (!active_queue || active_queue->IsEmpty())
    return PENDING_TREE;
  if (!pending_queue || pending_queue->IsEmpty())
    return ACTIVE_TREE;

  const Tile* active_tile = active_queue->Top();
  const Tile* pending_tile = pending_queue->Top();

  // If tiles are the same, it doesn't matter which tree we return.
  if (active_tile == pending_tile)
    return ACTIVE_TREE;

  const TilePriority& active_priority = active_tile->combined_priority();
  const TilePriority& pending_priority = pending_tile->combined_priority();

  // If the bins are the same and activation differs, then return the tree of
  // the tile not required for activation.
  if (active_priority.priority_bin == pending_priority.priority_bin &&
      active_tile->required_for_activation() !=
          pending_tile->required_for_activation()) {
    return active_tile->required_for_activation() ? PENDING_TREE : ACTIVE_TREE;
  }

  // Return tile with a lower priority.
  if (pending_priority.IsHigherPriorityThan(active_priority))
    return ACTIVE_TREE;
  return PENDING_TREE;
}

}  // namespace cc

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_tile_priority_queue.h"

namespace cc {

namespace {

class RasterOrderComparator {
 public:
  explicit RasterOrderComparator(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}

  bool operator()(
      const RasterTilePriorityQueue::PairedPictureLayerQueue* a,
      const RasterTilePriorityQueue::PairedPictureLayerQueue* b) const {
    // Note that in this function, we have to return true if and only if
    // b is strictly lower priority than a. Note that for the sake of
    // completeness, empty queue is considered to have lowest priority.
    if (a->IsEmpty() || b->IsEmpty())
      return b->IsEmpty() < a->IsEmpty();

    WhichTree a_tree = a->NextTileIteratorTree(tree_priority_);
    const PictureLayerImpl::LayerRasterTileIterator* a_iterator =
        a_tree == ACTIVE_TREE ? &a->active_iterator : &a->pending_iterator;

    WhichTree b_tree = b->NextTileIteratorTree(tree_priority_);
    const PictureLayerImpl::LayerRasterTileIterator* b_iterator =
        b_tree == ACTIVE_TREE ? &b->active_iterator : &b->pending_iterator;

    const Tile* a_tile = **a_iterator;
    const Tile* b_tile = **b_iterator;

    const TilePriority& a_priority =
        a_tile->priority_for_tree_priority(tree_priority_);
    const TilePriority& b_priority =
        b_tile->priority_for_tree_priority(tree_priority_);
    bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

    // If the bin is the same but the resolution is not, then the order will be
    // determined by whether we prioritize low res or not.
    // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
    // class but instead produced by the iterators.
    if (b_priority.priority_bin == a_priority.priority_bin &&
        b_priority.resolution != a_priority.resolution) {
      // Non ideal resolution should be sorted lower than other resolutions.
      if (a_priority.resolution == NON_IDEAL_RESOLUTION)
        return true;

      if (b_priority.resolution == NON_IDEAL_RESOLUTION)
        return false;

      if (prioritize_low_res)
        return b_priority.resolution == LOW_RESOLUTION;
      return b_priority.resolution == HIGH_RESOLUTION;
    }
    return b_priority.IsHigherPriorityThan(a_priority);
  }

 private:
  TreePriority tree_priority_;
};

}  // namespace

RasterTilePriorityQueue::RasterTilePriorityQueue() {
}

RasterTilePriorityQueue::~RasterTilePriorityQueue() {
}

void RasterTilePriorityQueue::Build(
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
  paired_queues_.make_heap(RasterOrderComparator(tree_priority_));
}

void RasterTilePriorityQueue::Reset() {
  paired_queues_.clear();
}

bool RasterTilePriorityQueue::IsEmpty() const {
  return paired_queues_.empty() || paired_queues_.front()->IsEmpty();
}

Tile* RasterTilePriorityQueue::Top() {
  DCHECK(!IsEmpty());
  return paired_queues_.front()->Top(tree_priority_);
}

void RasterTilePriorityQueue::Pop() {
  DCHECK(!IsEmpty());

  paired_queues_.pop_heap(RasterOrderComparator(tree_priority_));
  PairedPictureLayerQueue* paired_queue = paired_queues_.back();
  paired_queue->Pop(tree_priority_);
  paired_queues_.push_heap(RasterOrderComparator(tree_priority_));
}

RasterTilePriorityQueue::PairedPictureLayerQueue::PairedPictureLayerQueue() {
}

RasterTilePriorityQueue::PairedPictureLayerQueue::PairedPictureLayerQueue(
    const PictureLayerImpl::Pair& layer_pair,
    TreePriority tree_priority)
    : active_iterator(layer_pair.active
                          ? PictureLayerImpl::LayerRasterTileIterator(
                                layer_pair.active,
                                tree_priority == SMOOTHNESS_TAKES_PRIORITY)
                          : PictureLayerImpl::LayerRasterTileIterator()),
      pending_iterator(layer_pair.pending
                           ? PictureLayerImpl::LayerRasterTileIterator(
                                 layer_pair.pending,
                                 tree_priority == SMOOTHNESS_TAKES_PRIORITY)
                           : PictureLayerImpl::LayerRasterTileIterator()) {
}

RasterTilePriorityQueue::PairedPictureLayerQueue::~PairedPictureLayerQueue() {
}

bool RasterTilePriorityQueue::PairedPictureLayerQueue::IsEmpty() const {
  return !active_iterator && !pending_iterator;
}

Tile* RasterTilePriorityQueue::PairedPictureLayerQueue::Top(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  PictureLayerImpl::LayerRasterTileIterator* next_iterator =
      next_tree == ACTIVE_TREE ? &active_iterator : &pending_iterator;
  DCHECK(*next_iterator);
  Tile* tile = **next_iterator;
  DCHECK(std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   tile) == returned_shared_tiles.end());
  return tile;
}

void RasterTilePriorityQueue::PairedPictureLayerQueue::Pop(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  PictureLayerImpl::LayerRasterTileIterator* next_iterator =
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
RasterTilePriorityQueue::PairedPictureLayerQueue::NextTileIteratorTree(
    TreePriority tree_priority) const {
  DCHECK(!IsEmpty());

  // If we only have one iterator with tiles, return it.
  if (!active_iterator)
    return PENDING_TREE;
  if (!pending_iterator)
    return ACTIVE_TREE;

  // Now both iterators have tiles, so we have to decide based on tree priority.
  switch (tree_priority) {
    case SMOOTHNESS_TAKES_PRIORITY:
      return ACTIVE_TREE;
    case NEW_CONTENT_TAKES_PRIORITY:
      return PENDING_TREE;
    case SAME_PRIORITY_FOR_BOTH_TREES: {
      const Tile* active_tile = *active_iterator;
      const Tile* pending_tile = *pending_iterator;
      if (active_tile == pending_tile)
        return ACTIVE_TREE;

      const TilePriority& active_priority = active_tile->priority(ACTIVE_TREE);
      const TilePriority& pending_priority =
          pending_tile->priority(PENDING_TREE);

      if (active_priority.IsHigherPriorityThan(pending_priority))
        return ACTIVE_TREE;
      return PENDING_TREE;
    }
    default:
      NOTREACHED();
  }

  NOTREACHED();
  // Keep the compiler happy.
  return ACTIVE_TREE;
}

}  // namespace cc

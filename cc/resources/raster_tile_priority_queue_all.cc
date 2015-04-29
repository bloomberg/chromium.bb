// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_tile_priority_queue_all.h"

#include "cc/resources/tiling_set_raster_queue_all.h"

namespace cc {

namespace {

class RasterOrderComparator {
 public:
  explicit RasterOrderComparator(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}

  bool operator()(
      const RasterTilePriorityQueueAll::PairedTilingSetQueue* a,
      const RasterTilePriorityQueueAll::PairedTilingSetQueue* b) const {
    // Note that in this function, we have to return true if and only if
    // a is strictly lower priority than b. Note that for the sake of
    // completeness, empty queue is considered to have lowest priority.
    if (a->IsEmpty() || b->IsEmpty())
      return b->IsEmpty() < a->IsEmpty();

    WhichTree a_tree = a->NextTileIteratorTree(tree_priority_);
    const TilingSetRasterQueueAll* a_queue =
        a_tree == ACTIVE_TREE ? a->active_queue() : a->pending_queue();

    WhichTree b_tree = b->NextTileIteratorTree(tree_priority_);
    const TilingSetRasterQueueAll* b_queue =
        b_tree == ACTIVE_TREE ? b->active_queue() : b->pending_queue();

    const Tile* a_tile = a_queue->Top();
    const Tile* b_tile = b_queue->Top();

    const TilePriority& a_priority = a_tile->priority();
    const TilePriority& b_priority = b_tile->priority();
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

WhichTree HigherPriorityTree(TreePriority tree_priority,
                             const TilingSetRasterQueueAll* active_queue,
                             const TilingSetRasterQueueAll* pending_queue) {
  const Tile* active_tile = active_queue->Top();
  const Tile* pending_tile = pending_queue->Top();

  const TilePriority& active_priority = active_tile->priority();
  const TilePriority& pending_priority = pending_tile->priority();

  switch (tree_priority) {
    case SMOOTHNESS_TAKES_PRIORITY: {
      // If we're down to eventually bin tiles on the active tree, process the
      // pending tree to allow tiles required for activation to be initialized
      // when memory policy only allows prepaint.
      if (active_priority.priority_bin == TilePriority::EVENTUALLY &&
          pending_priority.priority_bin == TilePriority::NOW) {
        return PENDING_TREE;
      }
      return ACTIVE_TREE;
    }
    case NEW_CONTENT_TAKES_PRIORITY: {
      // If we're down to soon bin tiles on the pending tree, process the
      // active tree to allow tiles required for activation to be initialized
      // when memory policy only allows prepaint. Note that active required for
      // activation tiles might come from either now or soon bins.
      if (pending_priority.priority_bin >= TilePriority::SOON &&
          active_priority.priority_bin <= TilePriority::SOON) {
        return ACTIVE_TREE;
      }
      return PENDING_TREE;
    }
    case SAME_PRIORITY_FOR_BOTH_TREES: {
      if (active_priority.IsHigherPriorityThan(pending_priority))
        return ACTIVE_TREE;
      return PENDING_TREE;
    }
    default:
      NOTREACHED();
      return ACTIVE_TREE;
  }
}

scoped_ptr<TilingSetRasterQueueAll> CreateTilingSetRasterQueue(
    PictureLayerImpl* layer,
    TreePriority tree_priority) {
  if (!layer)
    return nullptr;
  PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
  bool prioritize_low_res = tree_priority == SMOOTHNESS_TAKES_PRIORITY;
  return make_scoped_ptr(
      new TilingSetRasterQueueAll(tiling_set, prioritize_low_res));
}

}  // namespace

RasterTilePriorityQueueAll::RasterTilePriorityQueueAll() {
}

RasterTilePriorityQueueAll::~RasterTilePriorityQueueAll() {
}

void RasterTilePriorityQueueAll::Build(
    const std::vector<PictureLayerImpl::Pair>& paired_layers,
    TreePriority tree_priority) {
  tree_priority_ = tree_priority;
  for (std::vector<PictureLayerImpl::Pair>::const_iterator it =
           paired_layers.begin();
       it != paired_layers.end(); ++it) {
    paired_queues_.push_back(
        make_scoped_ptr(new PairedTilingSetQueue(*it, tree_priority_)));
  }
  paired_queues_.make_heap(RasterOrderComparator(tree_priority_));
}

bool RasterTilePriorityQueueAll::IsEmpty() const {
  return paired_queues_.empty() || paired_queues_.front()->IsEmpty();
}

Tile* RasterTilePriorityQueueAll::Top() {
  DCHECK(!IsEmpty());
  return paired_queues_.front()->Top(tree_priority_);
}

void RasterTilePriorityQueueAll::Pop() {
  DCHECK(!IsEmpty());

  paired_queues_.pop_heap(RasterOrderComparator(tree_priority_));
  PairedTilingSetQueue* paired_queue = paired_queues_.back();
  paired_queue->Pop(tree_priority_);
  paired_queues_.push_heap(RasterOrderComparator(tree_priority_));
}

RasterTilePriorityQueueAll::PairedTilingSetQueue::PairedTilingSetQueue() {
}

RasterTilePriorityQueueAll::PairedTilingSetQueue::PairedTilingSetQueue(
    const PictureLayerImpl::Pair& layer_pair,
    TreePriority tree_priority)
    : active_queue_(
          CreateTilingSetRasterQueue(layer_pair.active, tree_priority)),
      pending_queue_(
          CreateTilingSetRasterQueue(layer_pair.pending, tree_priority)) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PairedTilingSetQueue::PairedTilingSetQueue",
                       TRACE_EVENT_SCOPE_THREAD, "state", StateAsValue());
}

RasterTilePriorityQueueAll::PairedTilingSetQueue::~PairedTilingSetQueue() {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PairedTilingSetQueue::~PairedTilingSetQueue",
                       TRACE_EVENT_SCOPE_THREAD, "state", StateAsValue());
}

bool RasterTilePriorityQueueAll::PairedTilingSetQueue::IsEmpty() const {
  return (!active_queue_ || active_queue_->IsEmpty()) &&
         (!pending_queue_ || pending_queue_->IsEmpty());
}

Tile* RasterTilePriorityQueueAll::PairedTilingSetQueue::Top(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  TilingSetRasterQueueAll* next_queue =
      next_tree == ACTIVE_TREE ? active_queue_.get() : pending_queue_.get();
  DCHECK(next_queue && !next_queue->IsEmpty());
  Tile* tile = next_queue->Top();
  DCHECK(returned_tiles_for_debug_.find(tile) ==
         returned_tiles_for_debug_.end());
  return tile;
}

void RasterTilePriorityQueueAll::PairedTilingSetQueue::Pop(
    TreePriority tree_priority) {
  DCHECK(!IsEmpty());

  WhichTree next_tree = NextTileIteratorTree(tree_priority);
  TilingSetRasterQueueAll* next_queue =
      next_tree == ACTIVE_TREE ? active_queue_.get() : pending_queue_.get();
  DCHECK(next_queue && !next_queue->IsEmpty());
  DCHECK(returned_tiles_for_debug_.insert(next_queue->Top()).second);
  next_queue->Pop();

  // If no empty, use Top to do DCHECK the next iterator.
  DCHECK(IsEmpty() || Top(tree_priority));
}

WhichTree
RasterTilePriorityQueueAll::PairedTilingSetQueue::NextTileIteratorTree(
    TreePriority tree_priority) const {
  DCHECK(!IsEmpty());

  // If we only have one queue with tiles, return it.
  if (!active_queue_ || active_queue_->IsEmpty())
    return PENDING_TREE;
  if (!pending_queue_ || pending_queue_->IsEmpty())
    return ACTIVE_TREE;

  // Now both iterators have tiles, so we have to decide based on tree priority.
  return HigherPriorityTree(tree_priority, active_queue_.get(),
                            pending_queue_.get());
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RasterTilePriorityQueueAll::PairedTilingSetQueue::StateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  bool active_queue_has_tile = active_queue_ && !active_queue_->IsEmpty();
  TilePriority::PriorityBin active_priority_bin = TilePriority::EVENTUALLY;
  TilePriority::PriorityBin pending_priority_bin = TilePriority::EVENTUALLY;
  if (active_queue_has_tile) {
    active_priority_bin = active_queue_->Top()->priority().priority_bin;
    pending_priority_bin = active_queue_->Top()->priority().priority_bin;
  }

  state->BeginDictionary("active_queue");
  state->SetBoolean("has_tile", active_queue_has_tile);
  state->SetInteger("active_priority_bin", active_priority_bin);
  state->SetInteger("pending_priority_bin", pending_priority_bin);
  state->EndDictionary();

  bool pending_queue_has_tile = pending_queue_ && !pending_queue_->IsEmpty();
  active_priority_bin = TilePriority::EVENTUALLY;
  pending_priority_bin = TilePriority::EVENTUALLY;
  if (pending_queue_has_tile) {
    active_priority_bin = pending_queue_->Top()->priority().priority_bin;
    pending_priority_bin = pending_queue_->Top()->priority().priority_bin;
  }

  state->BeginDictionary("pending_queue");
  state->SetBoolean("has_tile", active_queue_has_tile);
  state->SetInteger("active_priority_bin", active_priority_bin);
  state->SetInteger("pending_priority_bin", pending_priority_bin);
  state->EndDictionary();
  return state;
}

}  // namespace cc

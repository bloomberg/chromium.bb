// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_
#define CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_

#include <set>
#include <utility>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/tile_priority.h"
#include "cc/resources/tiling_set_raster_queue.h"

namespace cc {

// TODO(vmpstr): Consider virtualizing this and adding ::Create with the
// parameters of ::Build that would create a simpler queue for required only
// tiles (ie, there's no need for the heap if all we're interested in are the
// required tiles.
class CC_EXPORT RasterTilePriorityQueue {
 public:
  enum class Type { ALL, REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW };

  class PairedTilingSetQueue {
   public:
    PairedTilingSetQueue();
    PairedTilingSetQueue(const PictureLayerImpl::Pair& layer_pair,
                         TreePriority tree_priority,
                         Type type);
    ~PairedTilingSetQueue();

    bool IsEmpty() const;
    Tile* Top(TreePriority tree_priority);
    void Pop(TreePriority tree_priority);

    WhichTree NextTileIteratorTree(TreePriority tree_priority) const;
    void SkipTilesReturnedByTwin(TreePriority tree_priority);

    scoped_refptr<base::debug::ConvertableToTraceFormat> StateAsValue() const;

    const TilingSetRasterQueue* active_queue() const {
      return active_queue_.get();
    }
    const TilingSetRasterQueue* pending_queue() const {
      return pending_queue_.get();
    }

   private:
    scoped_ptr<TilingSetRasterQueue> active_queue_;
    scoped_ptr<TilingSetRasterQueue> pending_queue_;
    bool has_both_layers_;

    // Set of returned tiles (excluding the current one) for DCHECKing.
    std::set<const Tile*> returned_tiles_for_debug_;
  };

  RasterTilePriorityQueue();
  ~RasterTilePriorityQueue();

  void Build(const std::vector<PictureLayerImpl::Pair>& paired_layers,
             TreePriority tree_priority,
             Type type);
  void Reset();

  bool IsEmpty() const;
  Tile* Top();
  void Pop();

 private:
  // TODO(vmpstr): This is potentially unnecessary if it becomes the case that
  // PairedTilingSetQueue is fast enough to copy. In that case, we can use
  // objects directly (ie std::vector<PairedTilingSetQueue>.
  ScopedPtrVector<PairedTilingSetQueue> paired_queues_;
  TreePriority tree_priority_;

  DISALLOW_COPY_AND_ASSIGN(RasterTilePriorityQueue);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_

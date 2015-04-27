// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_
#define CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_

#include <set>
#include <utility>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/tile_priority.h"
#include "cc/resources/tiling_set_raster_queue_all.h"

namespace cc {

class CC_EXPORT RasterTilePriorityQueueAll : public RasterTilePriorityQueue {
 public:
  class PairedTilingSetQueue {
   public:
    PairedTilingSetQueue();
    PairedTilingSetQueue(const PictureLayerImpl::Pair& layer_pair,
                         TreePriority tree_priority);
    ~PairedTilingSetQueue();

    bool IsEmpty() const;
    Tile* Top(TreePriority tree_priority);
    void Pop(TreePriority tree_priority);

    WhichTree NextTileIteratorTree(TreePriority tree_priority) const;
    void SkipTilesReturnedByTwin(TreePriority tree_priority);

    scoped_refptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
        const;

    const TilingSetRasterQueueAll* active_queue() const {
      return active_queue_.get();
    }
    const TilingSetRasterQueueAll* pending_queue() const {
      return pending_queue_.get();
    }

   private:
    scoped_ptr<TilingSetRasterQueueAll> active_queue_;
    scoped_ptr<TilingSetRasterQueueAll> pending_queue_;
    bool has_both_layers_;

    // Set of returned tiles (excluding the current one) for DCHECKing.
    std::set<const Tile*> returned_tiles_for_debug_;
  };

  RasterTilePriorityQueueAll();
  ~RasterTilePriorityQueueAll() override;

  bool IsEmpty() const override;
  Tile* Top() override;
  void Pop() override;

 private:
  friend class RasterTilePriorityQueue;

  void Build(const std::vector<PictureLayerImpl::Pair>& paired_layers,
             TreePriority tree_priority);

  // TODO(vmpstr): This is potentially unnecessary if it becomes the case that
  // PairedTilingSetQueue is fast enough to copy. In that case, we can use
  // objects directly (ie std::vector<PairedTilingSetQueue>.
  ScopedPtrVector<PairedTilingSetQueue> paired_queues_;
  TreePriority tree_priority_;

  DISALLOW_COPY_AND_ASSIGN(RasterTilePriorityQueueAll);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_EVICTION_TILE_PRIORITY_QUEUE_H_
#define CC_RESOURCES_EVICTION_TILE_PRIORITY_QUEUE_H_

#include <set>
#include <utility>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/tile_priority.h"
#include "cc/resources/tiling_set_eviction_queue.h"

namespace cc {

class CC_EXPORT EvictionTilePriorityQueue {
 public:
  struct PairedTilingSetQueue {
    PairedTilingSetQueue();
    PairedTilingSetQueue(const PictureLayerImpl::Pair& layer_pair,
                         TreePriority tree_priority);
    ~PairedTilingSetQueue();

    bool IsEmpty() const;
    Tile* Top();
    void Pop();

    WhichTree NextTileIteratorTree() const;

    scoped_ptr<TilingSetEvictionQueue> active_queue;
    scoped_ptr<TilingSetEvictionQueue> pending_queue;

    // Set of returned tiles (excluding the current one) for DCHECKing.
    std::set<const Tile*> returned_tiles_for_debug;
  };

  EvictionTilePriorityQueue();
  ~EvictionTilePriorityQueue();

  void Build(const std::vector<PictureLayerImpl::Pair>& paired_layers,
             TreePriority tree_priority);

  bool IsEmpty() const;
  Tile* Top();
  void Pop();

 private:
  // TODO(vmpstr): This is potentially unnecessary if it becomes the case that
  // PairedTilingSetQueue is fast enough to copy. In that case, we can use
  // objects directly (ie std::vector<PairedTilingSetQueue>).
  ScopedPtrVector<PairedTilingSetQueue> paired_queues_;
  TreePriority tree_priority_;

  DISALLOW_COPY_AND_ASSIGN(EvictionTilePriorityQueue);
};

}  // namespace cc

#endif  // CC_RESOURCES_EVICTION_TILE_PRIORITY_QUEUE_H_

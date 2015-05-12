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
  RasterTilePriorityQueueAll();
  ~RasterTilePriorityQueueAll() override;

  bool IsEmpty() const override;
  const PrioritizedTile& Top() const override;
  void Pop() override;

 private:
  friend class RasterTilePriorityQueue;

  void Build(const std::vector<PictureLayerImpl*>& active_layers,
             const std::vector<PictureLayerImpl*>& pending_layers,
             TreePriority tree_priority);

  ScopedPtrVector<TilingSetRasterQueueAll>& GetNextQueues();
  const ScopedPtrVector<TilingSetRasterQueueAll>& GetNextQueues() const;

  ScopedPtrVector<TilingSetRasterQueueAll> active_queues_;
  ScopedPtrVector<TilingSetRasterQueueAll> pending_queues_;
  TreePriority tree_priority_;

  DISALLOW_COPY_AND_ASSIGN(RasterTilePriorityQueueAll);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_ALL_H_

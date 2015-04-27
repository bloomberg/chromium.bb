// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_
#define CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_

#include <vector>

#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/tiling_set_raster_queue_required.h"

namespace cc {
class Tile;

class RasterTilePriorityQueueRequired : public RasterTilePriorityQueue {
 public:
  RasterTilePriorityQueueRequired();
  ~RasterTilePriorityQueueRequired() override;

  bool IsEmpty() const override;
  Tile* Top() override;
  void Pop() override;

 private:
  friend class RasterTilePriorityQueue;

  void Build(const std::vector<PictureLayerImpl::Pair>& paired_layers,
             Type type);

  ScopedPtrVector<TilingSetRasterQueueRequired> tiling_set_queues_;

  DISALLOW_COPY_AND_ASSIGN(RasterTilePriorityQueueRequired);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_REQUIRED_H_

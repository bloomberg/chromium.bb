// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_
#define CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/tile_priority.h"

namespace cc {
class Tile;

class CC_EXPORT RasterTilePriorityQueue {
 public:
  enum class Type { ALL, REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW };

  // TODO(vmpstr): Make this work with PictureLayerTilingSet pairs instead.
  static scoped_ptr<RasterTilePriorityQueue> Create(
      const std::vector<PictureLayerImpl::Pair>& paired_layers,
      TreePriority tree_priority,
      Type type);

  virtual ~RasterTilePriorityQueue() {}

  virtual bool IsEmpty() const = 0;
  virtual Tile* Top() = 0;
  virtual void Pop() = 0;

 protected:
  RasterTilePriorityQueue() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RasterTilePriorityQueue);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_TILE_PRIORITY_QUEUE_H_

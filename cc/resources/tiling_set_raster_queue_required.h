// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_RASTER_QUEUE_REQUIRED_H_
#define CC_RESOURCES_TILING_SET_RASTER_QUEUE_REQUIRED_H_

#include "cc/base/cc_export.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/tile.h"

namespace cc {

// This queue only returns tiles that are required for either activation or
// draw, as specified by RasterTilePriorityQueue::Type passed in the
// constructor.
class CC_EXPORT TilingSetRasterQueueRequired {
 public:
  TilingSetRasterQueueRequired(PictureLayerTilingSet* tiling_set,
                               RasterTilePriorityQueue::Type type);
  ~TilingSetRasterQueueRequired();

  Tile* Top();
  const Tile* Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  // This iterator will return all tiles that are in the NOW bin on the given
  // tiling. The queue can then use these tiles and further filter them based on
  // whether they are required or not.
  class TilingIterator {
   public:
    TilingIterator();
    explicit TilingIterator(PictureLayerTiling* tiling,
                            TilingData* tiling_data);
    ~TilingIterator();

    bool done() const { return current_tile_ == nullptr; }
    const Tile* operator*() const { return current_tile_; }
    Tile* operator*() { return current_tile_; }
    TilingIterator& operator++();

   private:
    PictureLayerTiling* tiling_;
    TilingData* tiling_data_;

    Tile* current_tile_;
    TilingData::Iterator visible_iterator_;
  };

  bool IsTileRequired(const Tile* tile) const;

  TilingIterator iterator_;
  RasterTilePriorityQueue::Type type_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_RASTER_QUEUE_REQUIRED_H_

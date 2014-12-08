// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_
#define CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_

#include "cc/base/cc_export.h"
#include "cc/resources/picture_layer_tiling_set.h"

namespace cc {

class CC_EXPORT TilingSetRasterQueue {
 public:
  TilingSetRasterQueue();
  TilingSetRasterQueue(PictureLayerTilingSet* tiling_set,
                       bool prioritize_low_res);
  ~TilingSetRasterQueue();

  Tile* Top();
  const Tile* Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  enum IteratorType { LOW_RES, HIGH_RES, NUM_ITERATORS };

  void AdvanceToNextStage();

  PictureLayerTilingSet* tiling_set_;

  struct IterationStage {
    IteratorType iterator_type;
    TilePriority::PriorityBin tile_type;
  };

  size_t current_stage_;

  // One low res stage, and three high res stages.
  IterationStage stages_[4];
  PictureLayerTiling::TilingRasterTileIterator iterators_[NUM_ITERATORS];
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_

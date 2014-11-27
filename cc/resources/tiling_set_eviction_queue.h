// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_EVICTION_QUEUE_H_
#define CC_RESOURCES_TILING_SET_EVICTION_QUEUE_H_

#include "cc/base/cc_export.h"
#include "cc/resources/picture_layer_tiling_set.h"

namespace cc {

class CC_EXPORT TilingSetEvictionQueue {
 public:
  TilingSetEvictionQueue();
  TilingSetEvictionQueue(PictureLayerTilingSet* tiling_set,
                         TreePriority tree_priority);
  ~TilingSetEvictionQueue();

  Tile* Top();
  const Tile* Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  bool AdvanceToNextCategory();
  bool AdvanceToNextEvictionTile();
  bool AdvanceToNextTilingRangeType();
  bool AdvanceToNextValidTiling();

  PictureLayerTilingSet::TilingRange CurrentTilingRange() const;
  size_t CurrentTilingIndex() const;

  PictureLayerTilingSet* tiling_set_;
  TreePriority tree_priority_;

  PictureLayerTiling::EvictionCategory current_category_;
  size_t current_tiling_index_;
  PictureLayerTilingSet::TilingRangeType current_tiling_range_type_;
  Tile* current_eviction_tile_;

  const std::vector<Tile*>* eviction_tiles_;
  size_t next_eviction_tile_index_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_

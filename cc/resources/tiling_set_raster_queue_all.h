// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_RASTER_QUEUE_ALL_H_
#define CC_RESOURCES_TILING_SET_RASTER_QUEUE_ALL_H_

#include "cc/base/cc_export.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"

namespace cc {

// This queue returns all tiles required to be rasterized from HIGH_RESOLUTION
// and LOW_RESOLUTION tilings.
class CC_EXPORT TilingSetRasterQueueAll {
 public:
  TilingSetRasterQueueAll(PictureLayerTilingSet* tiling_set,
                          bool prioritize_low_res);
  ~TilingSetRasterQueueAll();

  Tile* Top();
  const Tile* Top() const;
  void Pop();
  bool IsEmpty() const;

 private:
  class TilingIterator {
   public:
    TilingIterator();
    explicit TilingIterator(PictureLayerTiling* tiling,
                            TilingData* tiling_data);
    ~TilingIterator();

    bool done() const { return current_tile_ == nullptr; }
    const Tile* operator*() const { return current_tile_; }
    Tile* operator*() { return current_tile_; }
    TilePriority::PriorityBin type() const {
      switch (phase_) {
        case VISIBLE_RECT:
          return TilePriority::NOW;
        case SKEWPORT_RECT:
        case SOON_BORDER_RECT:
          return TilePriority::SOON;
        case EVENTUALLY_RECT:
          return TilePriority::EVENTUALLY;
      }
      NOTREACHED();
      return TilePriority::EVENTUALLY;
    }

    TilingIterator& operator++();

   private:
    enum Phase {
      VISIBLE_RECT,
      SKEWPORT_RECT,
      SOON_BORDER_RECT,
      EVENTUALLY_RECT
    };

    void AdvancePhase();
    bool TileNeedsRaster(Tile* tile) const {
      return tile->NeedsRaster() && !tiling_->IsTileOccluded(tile);
    }

    PictureLayerTiling* tiling_;
    TilingData* tiling_data_;

    Phase phase_;

    Tile* current_tile_;
    TilingData::Iterator visible_iterator_;
    TilingData::SpiralDifferenceIterator spiral_iterator_;
  };

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
  TilingIterator iterators_[NUM_ITERATORS];
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_RASTER_QUEUE_ALL_H_

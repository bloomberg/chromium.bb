// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tiling_set_raster_queue.h"

namespace cc {

TilingSetRasterQueue::TilingSetRasterQueue()
    : tiling_set_(nullptr), current_stage_(arraysize(stages_)) {
}

TilingSetRasterQueue::TilingSetRasterQueue(PictureLayerTilingSet* tiling_set,
                                           bool prioritize_low_res)
    : tiling_set_(tiling_set), current_stage_(0) {
  DCHECK(tiling_set_);

  // Early out if the tiling set has no tilings.
  if (!tiling_set_->num_tilings()) {
    current_stage_ = arraysize(stages_);
    return;
  }

  // Find high and low res tilings and initialize the iterators.
  for (size_t i = 0; i < tiling_set_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tiling_set_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION) {
      iterators_[HIGH_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling);
    }

    if (prioritize_low_res && tiling->resolution() == LOW_RESOLUTION) {
      iterators_[LOW_RES] =
          PictureLayerTiling::TilingRasterTileIterator(tiling);
    }
  }

  if (prioritize_low_res) {
    stages_[0].iterator_type = LOW_RES;
    stages_[0].tile_type = TilePriority::NOW;

    stages_[1].iterator_type = HIGH_RES;
    stages_[1].tile_type = TilePriority::NOW;
  } else {
    stages_[0].iterator_type = HIGH_RES;
    stages_[0].tile_type = TilePriority::NOW;

    stages_[1].iterator_type = LOW_RES;
    stages_[1].tile_type = TilePriority::NOW;
  }

  stages_[2].iterator_type = HIGH_RES;
  stages_[2].tile_type = TilePriority::SOON;

  stages_[3].iterator_type = HIGH_RES;
  stages_[3].tile_type = TilePriority::EVENTUALLY;

  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;
  if (!iterators_[index] || iterators_[index].get_type() != tile_type)
    AdvanceToNextStage();
}

TilingSetRasterQueue::~TilingSetRasterQueue() {
}

bool TilingSetRasterQueue::IsEmpty() const {
  return current_stage_ >= arraysize(stages_);
}

void TilingSetRasterQueue::Pop() {
  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

  // First advance the iterator.
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == tile_type);
  ++iterators_[index];

  if (!iterators_[index] || iterators_[index].get_type() != tile_type)
    AdvanceToNextStage();
}

Tile* TilingSetRasterQueue::Top() {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

const Tile* TilingSetRasterQueue::Top() const {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(iterators_[index].get_type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

void TilingSetRasterQueue::AdvanceToNextStage() {
  DCHECK_LT(current_stage_, arraysize(stages_));
  ++current_stage_;
  while (current_stage_ < arraysize(stages_)) {
    IteratorType index = stages_[current_stage_].iterator_type;
    TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

    if (iterators_[index] && iterators_[index].get_type() == tile_type)
      break;
    ++current_stage_;
  }
}

}  // namespace cc

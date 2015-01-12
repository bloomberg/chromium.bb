// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tiling_set_raster_queue_all.h"

#include <utility>

#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"

namespace cc {

TilingSetRasterQueueAll::TilingSetRasterQueueAll(
    PictureLayerTilingSet* tiling_set,
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
    if (tiling->resolution() == HIGH_RESOLUTION)
      iterators_[HIGH_RES] = TilingIterator(tiling, &tiling->tiling_data_);
    if (prioritize_low_res && tiling->resolution() == LOW_RESOLUTION)
      iterators_[LOW_RES] = TilingIterator(tiling, &tiling->tiling_data_);
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
  if (iterators_[index].done() || iterators_[index].type() != tile_type)
    AdvanceToNextStage();
}

TilingSetRasterQueueAll::~TilingSetRasterQueueAll() {
}

bool TilingSetRasterQueueAll::IsEmpty() const {
  return current_stage_ >= arraysize(stages_);
}

void TilingSetRasterQueueAll::Pop() {
  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

  // First advance the iterator.
  DCHECK(!iterators_[index].done());
  DCHECK(iterators_[index].type() == tile_type);
  ++iterators_[index];

  if (iterators_[index].done() || iterators_[index].type() != tile_type)
    AdvanceToNextStage();
}

Tile* TilingSetRasterQueueAll::Top() {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(!iterators_[index].done());
  DCHECK(iterators_[index].type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

const Tile* TilingSetRasterQueueAll::Top() const {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(!iterators_[index].done());
  DCHECK(iterators_[index].type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

void TilingSetRasterQueueAll::AdvanceToNextStage() {
  DCHECK_LT(current_stage_, arraysize(stages_));
  ++current_stage_;
  while (current_stage_ < arraysize(stages_)) {
    IteratorType index = stages_[current_stage_].iterator_type;
    TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

    if (!iterators_[index].done() && iterators_[index].type() == tile_type)
      break;
    ++current_stage_;
  }
}

TilingSetRasterQueueAll::TilingIterator::TilingIterator()
    : tiling_(NULL), current_tile_(NULL) {
}

TilingSetRasterQueueAll::TilingIterator::TilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : tiling_(tiling),
      tiling_data_(tiling_data),
      phase_(VISIBLE_RECT),
      current_tile_(NULL) {
  if (!tiling_->has_visible_rect_tiles()) {
    AdvancePhase();
    return;
  }

  visible_iterator_ =
      TilingData::Iterator(tiling_data_, tiling_->current_visible_rect(),
                           false /* include_borders */);
  if (!visible_iterator_) {
    AdvancePhase();
    return;
  }

  current_tile_ =
      tiling_->TileAt(visible_iterator_.index_x(), visible_iterator_.index_y());
  if (!current_tile_ || !TileNeedsRaster(current_tile_)) {
    ++(*this);
    return;
  }
  tiling_->UpdateTileAndTwinPriority(current_tile_);
}

TilingSetRasterQueueAll::TilingIterator::~TilingIterator() {
}

void TilingSetRasterQueueAll::TilingIterator::AdvancePhase() {
  DCHECK_LT(phase_, EVENTUALLY_RECT);

  do {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case VISIBLE_RECT:
        NOTREACHED();
        return;
      case SKEWPORT_RECT:
        if (!tiling_->has_skewport_rect_tiles())
          continue;

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            tiling_data_, tiling_->current_skewport_rect(),
            tiling_->current_visible_rect(), tiling_->current_visible_rect());
        break;
      case SOON_BORDER_RECT:
        if (!tiling_->has_soon_border_rect_tiles())
          continue;

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            tiling_data_, tiling_->current_soon_border_rect(),
            tiling_->current_skewport_rect(), tiling_->current_visible_rect());
        break;
      case EVENTUALLY_RECT:
        if (!tiling_->has_eventually_rect_tiles()) {
          current_tile_ = NULL;
          return;
        }

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            tiling_data_, tiling_->current_eventually_rect(),
            tiling_->current_skewport_rect(),
            tiling_->current_soon_border_rect());
        break;
    }

    while (spiral_iterator_) {
      current_tile_ = tiling_->TileAt(spiral_iterator_.index_x(),
                                      spiral_iterator_.index_y());
      if (current_tile_ && TileNeedsRaster(current_tile_))
        break;
      ++spiral_iterator_;
    }

    if (!spiral_iterator_ && phase_ == EVENTUALLY_RECT) {
      current_tile_ = NULL;
      break;
    }
  } while (!spiral_iterator_);

  if (current_tile_)
    tiling_->UpdateTileAndTwinPriority(current_tile_);
}

TilingSetRasterQueueAll::TilingIterator&
    TilingSetRasterQueueAll::TilingIterator::
    operator++() {
  current_tile_ = NULL;
  while (!current_tile_ || !TileNeedsRaster(current_tile_)) {
    std::pair<int, int> next_index;
    switch (phase_) {
      case VISIBLE_RECT:
        ++visible_iterator_;
        if (!visible_iterator_) {
          AdvancePhase();
          return *this;
        }
        next_index = visible_iterator_.index();
        break;
      case SKEWPORT_RECT:
      case SOON_BORDER_RECT:
        ++spiral_iterator_;
        if (!spiral_iterator_) {
          AdvancePhase();
          return *this;
        }
        next_index = spiral_iterator_.index();
        break;
      case EVENTUALLY_RECT:
        ++spiral_iterator_;
        if (!spiral_iterator_) {
          current_tile_ = NULL;
          return *this;
        }
        next_index = spiral_iterator_.index();
        break;
    }
    current_tile_ = tiling_->TileAt(next_index.first, next_index.second);
  }

  if (current_tile_)
    tiling_->UpdateTileAndTwinPriority(current_tile_);
  return *this;
}

}  // namespace cc

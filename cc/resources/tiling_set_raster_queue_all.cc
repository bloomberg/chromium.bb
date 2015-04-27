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

// OnePriorityRectIterator
TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator()
    : tile_(nullptr), tiling_(nullptr), tiling_data_(nullptr) {
}

TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : tile_(nullptr), tiling_(tiling), tiling_data_(tiling_data) {
}

template <typename TilingIteratorType>
void TilingSetRasterQueueAll::OnePriorityRectIterator::AdvanceToNextTile(
    TilingIteratorType* iterator) {
  tile_ = nullptr;
  while (!tile_ || !TileNeedsRaster(tile_)) {
    ++(*iterator);
    if (!(*iterator)) {
      tile_ = nullptr;
      return;
    }
    tile_ = tiling_->TileAt(iterator->index_x(), iterator->index_y());
  }
  tiling_->UpdateTileAndTwinPriority(tile_);
}

template <typename TilingIteratorType>
bool TilingSetRasterQueueAll::OnePriorityRectIterator::
    GetFirstTileAndCheckIfValid(TilingIteratorType* iterator) {
  tile_ = tiling_->TileAt(iterator->index_x(), iterator->index_y());
  if (!tile_ || !TileNeedsRaster(tile_)) {
    tile_ = nullptr;
    return false;
  }
  tiling_->UpdateTileAndTwinPriority(tile_);
  return true;
}

// VisibleTilingIterator.
TilingSetRasterQueueAll::VisibleTilingIterator::VisibleTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data) {
  if (!tiling_->has_visible_rect_tiles())
    return;
  iterator_ =
      TilingData::Iterator(tiling_data_, tiling_->current_visible_rect(),
                           false /* include_borders */);
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::VisibleTilingIterator&
    TilingSetRasterQueueAll::VisibleTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// SkewportTilingIterator.
TilingSetRasterQueueAll::SkewportTilingIterator::SkewportTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data) {
  if (!tiling_->has_skewport_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_skewport_rect(),
      tiling_->current_visible_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::SkewportTilingIterator&
    TilingSetRasterQueueAll::SkewportTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// SoonBorderTilingIterator.
TilingSetRasterQueueAll::SoonBorderTilingIterator::SoonBorderTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data) {
  if (!tiling_->has_soon_border_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_soon_border_rect(),
      tiling_->current_skewport_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::SoonBorderTilingIterator&
    TilingSetRasterQueueAll::SoonBorderTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// EventuallyTilingIterator.
TilingSetRasterQueueAll::EventuallyTilingIterator::EventuallyTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data) {
  if (!tiling_->has_eventually_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_eventually_rect(),
      tiling_->current_skewport_rect(), tiling_->current_soon_border_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::EventuallyTilingIterator&
    TilingSetRasterQueueAll::EventuallyTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// TilingIterator
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
  visible_iterator_ = VisibleTilingIterator(tiling_, tiling_data_);
  if (visible_iterator_.done()) {
    AdvancePhase();
    return;
  }
  current_tile_ = *visible_iterator_;
}

void TilingSetRasterQueueAll::TilingIterator::AdvancePhase() {
  DCHECK_LT(phase_, EVENTUALLY_RECT);

  current_tile_ = nullptr;
  while (!current_tile_ && phase_ < EVENTUALLY_RECT) {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case VISIBLE_RECT:
        NOTREACHED();
        return;
      case SKEWPORT_RECT:
        skewport_iterator_ = SkewportTilingIterator(tiling_, tiling_data_);
        if (!skewport_iterator_.done())
          current_tile_ = *skewport_iterator_;
        break;
      case SOON_BORDER_RECT:
        soon_border_iterator_ = SoonBorderTilingIterator(tiling_, tiling_data_);
        if (!soon_border_iterator_.done())
          current_tile_ = *soon_border_iterator_;
        break;
      case EVENTUALLY_RECT:
        eventually_iterator_ = EventuallyTilingIterator(tiling_, tiling_data_);
        if (!eventually_iterator_.done())
          current_tile_ = *eventually_iterator_;
        break;
    }
  }
}

TilingSetRasterQueueAll::TilingIterator&
    TilingSetRasterQueueAll::TilingIterator::
    operator++() {
  switch (phase_) {
    case VISIBLE_RECT:
      ++visible_iterator_;
      if (visible_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *visible_iterator_;
      break;
    case SKEWPORT_RECT:
      ++skewport_iterator_;
      if (skewport_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *skewport_iterator_;
      break;
    case SOON_BORDER_RECT:
      ++soon_border_iterator_;
      if (soon_border_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *soon_border_iterator_;
      break;
    case EVENTUALLY_RECT:
      ++eventually_iterator_;
      if (eventually_iterator_.done()) {
        current_tile_ = nullptr;
        return *this;
      }
      current_tile_ = *eventually_iterator_;
      break;
  }
  return *this;
}

}  // namespace cc

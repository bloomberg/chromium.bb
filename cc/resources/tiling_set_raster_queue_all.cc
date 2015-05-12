// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tiling_set_raster_queue_all.h"

#include <utility>

#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"

namespace cc {

TilingSetRasterQueueAll::IterationStage::IterationStage(
    IteratorType type,
    TilePriority::PriorityBin bin)
    : iterator_type(type), tile_type(bin) {
}

TilingSetRasterQueueAll::TilingSetRasterQueueAll(
    PictureLayerTilingSet* tiling_set,
    bool prioritize_low_res)
    : tiling_set_(tiling_set), current_stage_(0) {
  DCHECK(tiling_set_);

  // Early out if the tiling set has no tilings.
  if (!tiling_set_->num_tilings())
    return;

  const PictureLayerTilingClient* client = tiling_set->client();
  WhichTree tree = tiling_set->tree();
  // Find high and low res tilings and initialize the iterators.
  PictureLayerTiling* high_res_tiling = nullptr;
  PictureLayerTiling* low_res_tiling = nullptr;
  // This variable would point to a tiling that has a NON_IDEAL_RESOLUTION
  // resolution on the active tree, but HIGH_RESOLUTION on the pending tree.
  // These tilings are the only non-ideal tilings that could have required for
  // activation tiles, so they need to be considered for rasterization.
  PictureLayerTiling* active_non_ideal_pending_high_res_tiling = nullptr;
  for (size_t i = 0; i < tiling_set_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tiling_set_->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION)
      high_res_tiling = tiling;
    if (prioritize_low_res && tiling->resolution() == LOW_RESOLUTION)
      low_res_tiling = tiling;
    if (tree == ACTIVE_TREE && tiling->resolution() == NON_IDEAL_RESOLUTION) {
      const PictureLayerTiling* twin =
          client->GetPendingOrActiveTwinTiling(tiling);
      if (twin && twin->resolution() == HIGH_RESOLUTION)
        active_non_ideal_pending_high_res_tiling = tiling;
    }
  }

  bool use_low_res_tiling = low_res_tiling && low_res_tiling->has_tiles();
  if (use_low_res_tiling && prioritize_low_res) {
    iterators_[LOW_RES] =
        TilingIterator(low_res_tiling, &low_res_tiling->tiling_data_);
    stages_->push_back(IterationStage(LOW_RES, TilePriority::NOW));
  }

  bool use_high_res_tiling = high_res_tiling && high_res_tiling->has_tiles();
  if (use_high_res_tiling) {
    iterators_[HIGH_RES] =
        TilingIterator(high_res_tiling, &high_res_tiling->tiling_data_);
    stages_->push_back(IterationStage(HIGH_RES, TilePriority::NOW));
  }

  if (low_res_tiling && !prioritize_low_res) {
    iterators_[LOW_RES] =
        TilingIterator(low_res_tiling, &low_res_tiling->tiling_data_);
    stages_->push_back(IterationStage(LOW_RES, TilePriority::NOW));
  }

  if (active_non_ideal_pending_high_res_tiling &&
      active_non_ideal_pending_high_res_tiling->has_tiles()) {
    iterators_[ACTIVE_NON_IDEAL_PENDING_HIGH_RES] =
        TilingIterator(active_non_ideal_pending_high_res_tiling,
                       &active_non_ideal_pending_high_res_tiling->tiling_data_);

    stages_->push_back(
        IterationStage(ACTIVE_NON_IDEAL_PENDING_HIGH_RES, TilePriority::NOW));
    stages_->push_back(
        IterationStage(ACTIVE_NON_IDEAL_PENDING_HIGH_RES, TilePriority::SOON));
  }

  if (use_high_res_tiling) {
    stages_->push_back(IterationStage(HIGH_RES, TilePriority::SOON));
    stages_->push_back(IterationStage(HIGH_RES, TilePriority::EVENTUALLY));
  }

  if (stages_->empty())
    return;

  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;
  if (iterators_[index].done() || iterators_[index].type() != tile_type)
    AdvanceToNextStage();
}

TilingSetRasterQueueAll::~TilingSetRasterQueueAll() {
}

bool TilingSetRasterQueueAll::IsEmpty() const {
  return current_stage_ >= stages_->size();
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

const PrioritizedTile& TilingSetRasterQueueAll::Top() const {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(!iterators_[index].done());
  DCHECK(iterators_[index].type() == stages_[current_stage_].tile_type);

  return *iterators_[index];
}

void TilingSetRasterQueueAll::AdvanceToNextStage() {
  DCHECK_LT(current_stage_, stages_->size());
  ++current_stage_;
  while (current_stage_ < stages_->size()) {
    IteratorType index = stages_[current_stage_].iterator_type;
    TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

    if (!iterators_[index].done() && iterators_[index].type() == tile_type)
      break;
    ++current_stage_;
  }
}

// OnePriorityRectIterator
TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator()
    : tiling_(nullptr), tiling_data_(nullptr) {
}

TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : tiling_(tiling), tiling_data_(tiling_data) {
}

template <typename TilingIteratorType>
void TilingSetRasterQueueAll::OnePriorityRectIterator::AdvanceToNextTile(
    TilingIteratorType* iterator) {
  current_tile_ = PrioritizedTile();
  Tile* tile = nullptr;
  while (!tile || !TileNeedsRaster(tile)) {
    ++(*iterator);
    if (!(*iterator)) {
      return;
    }
    tile = tiling_->TileAt(iterator->index_x(), iterator->index_y());
  }
  tiling_->UpdateRequiredStatesOnTile(tile);
  current_tile_ = tiling_->MakePrioritizedTile(tile);
}

template <typename TilingIteratorType>
bool TilingSetRasterQueueAll::OnePriorityRectIterator::
    GetFirstTileAndCheckIfValid(TilingIteratorType* iterator) {
  Tile* tile = tiling_->TileAt(iterator->index_x(), iterator->index_y());
  if (!tile || !TileNeedsRaster(tile)) {
    current_tile_ = PrioritizedTile();
    return false;
  }
  tiling_->UpdateRequiredStatesOnTile(tile);
  current_tile_ = tiling_->MakePrioritizedTile(tile);
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

// PendingVisibleTilingIterator.
TilingSetRasterQueueAll::PendingVisibleTilingIterator::
    PendingVisibleTilingIterator(PictureLayerTiling* tiling,
                                 TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data) {
  iterator_ = TilingData::DifferenceIterator(tiling_data_,
                                             tiling_->pending_visible_rect(),
                                             tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::PendingVisibleTilingIterator&
    TilingSetRasterQueueAll::PendingVisibleTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// SkewportTilingIterator.
TilingSetRasterQueueAll::SkewportTilingIterator::SkewportTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data),
      pending_visible_rect_(tiling->pending_visible_rect()) {
  if (!tiling_->has_skewport_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_skewport_rect(),
      tiling_->current_visible_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
    ++(*this);
}

TilingSetRasterQueueAll::SkewportTilingIterator&
    TilingSetRasterQueueAll::SkewportTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  while (!done()) {
    if (!current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
      break;
    AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// SoonBorderTilingIterator.
TilingSetRasterQueueAll::SoonBorderTilingIterator::SoonBorderTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data),
      pending_visible_rect_(tiling->pending_visible_rect()) {
  if (!tiling_->has_soon_border_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_soon_border_rect(),
      tiling_->current_skewport_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
    ++(*this);
}

TilingSetRasterQueueAll::SoonBorderTilingIterator&
    TilingSetRasterQueueAll::SoonBorderTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  while (!done()) {
    if (!current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
      break;
    AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// EventuallyTilingIterator.
TilingSetRasterQueueAll::EventuallyTilingIterator::EventuallyTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling, tiling_data),
      pending_visible_rect_(tiling->pending_visible_rect()) {
  if (!tiling_->has_eventually_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_eventually_rect(),
      tiling_->current_skewport_rect(), tiling_->current_soon_border_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
    ++(*this);
}

TilingSetRasterQueueAll::EventuallyTilingIterator&
    TilingSetRasterQueueAll::EventuallyTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  while (!done()) {
    if (!current_tile_.tile()->content_rect().Intersects(pending_visible_rect_))
      break;
    AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// TilingIterator
TilingSetRasterQueueAll::TilingIterator::TilingIterator() : tiling_(nullptr) {
}

TilingSetRasterQueueAll::TilingIterator::TilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : tiling_(tiling), tiling_data_(tiling_data), phase_(VISIBLE_RECT) {
  visible_iterator_ = VisibleTilingIterator(tiling_, tiling_data_);
  if (visible_iterator_.done()) {
    AdvancePhase();
    return;
  }
  current_tile_ = *visible_iterator_;
}

TilingSetRasterQueueAll::TilingIterator::~TilingIterator() {
}

void TilingSetRasterQueueAll::TilingIterator::AdvancePhase() {
  DCHECK_LT(phase_, EVENTUALLY_RECT);

  current_tile_ = PrioritizedTile();
  while (!current_tile_.tile() && phase_ < EVENTUALLY_RECT) {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case VISIBLE_RECT:
        NOTREACHED();
        return;
      case PENDING_VISIBLE_RECT:
        pending_visible_iterator_ =
            PendingVisibleTilingIterator(tiling_, tiling_data_);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
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
    case PENDING_VISIBLE_RECT:
      ++pending_visible_iterator_;
      if (pending_visible_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *pending_visible_iterator_;
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
        current_tile_ = PrioritizedTile();
        return *this;
      }
      current_tile_ = *eventually_iterator_;
      break;
  }
  return *this;
}

}  // namespace cc

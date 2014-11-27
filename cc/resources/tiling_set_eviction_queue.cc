// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tiling_set_eviction_queue.h"

namespace cc {

TilingSetEvictionQueue::TilingSetEvictionQueue()
    : tiling_set_(nullptr),
      tree_priority_(SAME_PRIORITY_FOR_BOTH_TREES),
      current_category_(PictureLayerTiling::EVENTUALLY),
      current_tiling_index_(0u),
      current_tiling_range_type_(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES),
      current_eviction_tile_(nullptr),
      eviction_tiles_(nullptr),
      next_eviction_tile_index_(0u) {
}

TilingSetEvictionQueue::TilingSetEvictionQueue(
    PictureLayerTilingSet* tiling_set,
    TreePriority tree_priority)
    : tiling_set_(tiling_set),
      tree_priority_(tree_priority),
      current_category_(PictureLayerTiling::EVENTUALLY),
      current_tiling_index_(0u),
      current_tiling_range_type_(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES),
      current_eviction_tile_(nullptr),
      eviction_tiles_(nullptr),
      next_eviction_tile_index_(0u) {
  DCHECK(tiling_set_);

  // Early out if the layer has no tilings.
  if (!tiling_set_->num_tilings())
    return;

  current_tiling_index_ = CurrentTilingRange().start - 1u;
  AdvanceToNextValidTiling();
}

TilingSetEvictionQueue::~TilingSetEvictionQueue() {
}

bool TilingSetEvictionQueue::IsEmpty() const {
  return !current_eviction_tile_;
}

void TilingSetEvictionQueue::Pop() {
  DCHECK(!IsEmpty());

  if (!AdvanceToNextEvictionTile())
    AdvanceToNextValidTiling();
}

Tile* TilingSetEvictionQueue::Top() {
  DCHECK(!IsEmpty());
  return current_eviction_tile_;
}

const Tile* TilingSetEvictionQueue::Top() const {
  DCHECK(!IsEmpty());
  return current_eviction_tile_;
}

bool TilingSetEvictionQueue::AdvanceToNextCategory() {
  // Advance to the next category. This is done only after all tiling range
  // types within the previous category have been gone through.
  DCHECK_EQ(current_tiling_range_type_, PictureLayerTilingSet::HIGH_RES);

  switch (current_category_) {
    case PictureLayerTiling::EVENTUALLY:
      current_category_ =
          PictureLayerTiling::EVENTUALLY_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::EVENTUALLY_AND_REQUIRED_FOR_ACTIVATION:
      current_category_ = PictureLayerTiling::SOON;
      return true;
    case PictureLayerTiling::SOON:
      current_category_ = PictureLayerTiling::SOON_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::SOON_AND_REQUIRED_FOR_ACTIVATION:
      current_category_ = PictureLayerTiling::NOW;
      return true;
    case PictureLayerTiling::NOW:
      current_category_ = PictureLayerTiling::NOW_AND_REQUIRED_FOR_ACTIVATION;
      return true;
    case PictureLayerTiling::NOW_AND_REQUIRED_FOR_ACTIVATION:
      return false;
  }
  NOTREACHED();
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextEvictionTile() {
  // Advance to the next eviction tile within the current category and tiling.
  // This is done while advancing to a new tiling (in which case the next
  // eviction tile index is 0) and while popping the current tile (in which
  // case the next eviction tile index is greater than 0).
  DCHECK_EQ(next_eviction_tile_index_ > 0, current_eviction_tile_ != nullptr);

  while (next_eviction_tile_index_ < eviction_tiles_->size()) {
    Tile* tile = (*eviction_tiles_)[next_eviction_tile_index_];
    ++next_eviction_tile_index_;
    if (tile->HasResources()) {
      current_eviction_tile_ = tile;
      return true;
    }
  }

  current_eviction_tile_ = nullptr;
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextTilingRangeType() {
  // Advance to the next tiling range type within the current category or to
  // the first tiling range type within the next category. This is done only
  // after all tilings within the previous tiling range type have been gone
  // through.
  DCHECK_EQ(current_tiling_index_, CurrentTilingRange().end);

  switch (current_tiling_range_type_) {
    case PictureLayerTilingSet::HIGHER_THAN_HIGH_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::LOWER_THAN_LOW_RES;
      return true;
    case PictureLayerTilingSet::LOWER_THAN_LOW_RES:
      current_tiling_range_type_ =
          PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES;
      return true;
    case PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::LOW_RES;
      return true;
    case PictureLayerTilingSet::LOW_RES:
      current_tiling_range_type_ = PictureLayerTilingSet::HIGH_RES;
      return true;
    case PictureLayerTilingSet::HIGH_RES:
      if (!AdvanceToNextCategory())
        return false;

      current_tiling_range_type_ = PictureLayerTilingSet::HIGHER_THAN_HIGH_RES;
      return true;
  }
  NOTREACHED();
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextValidTiling() {
  // Advance to the next tiling within current tiling range type or to
  // the first tiling within the next tiling range type or category until
  // the next eviction tile is found. This is done only after all eviction
  // tiles within the previous tiling within the current category and tiling
  // range type have been gone through.
  DCHECK(!current_eviction_tile_);
  DCHECK_NE(current_tiling_index_, CurrentTilingRange().end);

  for (;;) {
    ++current_tiling_index_;
    while (current_tiling_index_ == CurrentTilingRange().end) {
      if (!AdvanceToNextTilingRangeType())
        return false;
      current_tiling_index_ = CurrentTilingRange().start;
    }

    PictureLayerTiling* tiling = tiling_set_->tiling_at(CurrentTilingIndex());
    eviction_tiles_ =
        tiling->GetEvictionTiles(tree_priority_, current_category_);
    next_eviction_tile_index_ = 0u;
    if (AdvanceToNextEvictionTile())
      return true;
  }
}

PictureLayerTilingSet::TilingRange
TilingSetEvictionQueue::CurrentTilingRange() const {
  return tiling_set_->GetTilingRange(current_tiling_range_type_);
}

size_t TilingSetEvictionQueue::CurrentTilingIndex() const {
  DCHECK_NE(current_tiling_index_, CurrentTilingRange().end);
  switch (current_tiling_range_type_) {
    case PictureLayerTilingSet::HIGHER_THAN_HIGH_RES:
    case PictureLayerTilingSet::LOW_RES:
    case PictureLayerTilingSet::HIGH_RES:
      return current_tiling_index_;
    // Tilings in the following ranges are accessed in reverse order.
    case PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES:
    case PictureLayerTilingSet::LOWER_THAN_LOW_RES: {
      PictureLayerTilingSet::TilingRange tiling_range = CurrentTilingRange();
      size_t current_tiling_range_offset =
          current_tiling_index_ - tiling_range.start;
      return tiling_range.end - 1 - current_tiling_range_offset;
    }
  }
  NOTREACHED();
  return 0;
}

}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tiling_set_raster_queue_required.h"

#include <utility>

#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"

namespace cc {

TilingSetRasterQueueRequired::TilingSetRasterQueueRequired(
    PictureLayerTilingSet* tiling_set,
    RasterTilePriorityQueue::Type type)
    : type_(type) {
  DCHECK_NE(static_cast<int>(type),
            static_cast<int>(RasterTilePriorityQueue::Type::ALL));

  // Required tiles should only come from HIGH_RESOLUTION tilings. However, if
  // we want required for activation tiles on the active tree, then it will come
  // from tilings whose pending twin is high resolution.
  PictureLayerTiling* tiling = nullptr;
  if (type == RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION &&
      tiling_set->client()->GetTree() == ACTIVE_TREE) {
    for (size_t i = 0; i < tiling_set->num_tilings(); ++i) {
      PictureLayerTiling* active_tiling = tiling_set->tiling_at(i);
      const PictureLayerTiling* pending_twin =
          tiling_set->client()->GetPendingOrActiveTwinTiling(active_tiling);
      if (pending_twin && pending_twin->resolution() == HIGH_RESOLUTION) {
        tiling = active_tiling;
        break;
      }
    }
  } else {
    tiling = tiling_set->FindTilingWithResolution(HIGH_RESOLUTION);
  }

  // If we don't have a tiling, then this queue will yield no tiles. See
  // PictureLayerImpl::CanHaveTilings for examples of when a HIGH_RESOLUTION
  // tiling would not be generated.
  if (!tiling)
    return;

  if (type == RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION) {
    iterator_ = TilingIterator(tiling, &tiling->tiling_data_,
                               tiling->pending_visible_rect());
  } else {
    iterator_ = TilingIterator(tiling, &tiling->tiling_data_,
                               tiling->current_visible_rect());
  }

  while (!iterator_.done() && !IsTileRequired(*iterator_))
    ++iterator_;
}

TilingSetRasterQueueRequired::~TilingSetRasterQueueRequired() {
}

bool TilingSetRasterQueueRequired::IsEmpty() const {
  return iterator_.done();
}

void TilingSetRasterQueueRequired::Pop() {
  DCHECK(!IsEmpty());
  ++iterator_;
  while (!iterator_.done() && !IsTileRequired(*iterator_))
    ++iterator_;
}

Tile* TilingSetRasterQueueRequired::Top() {
  DCHECK(!IsEmpty());
  return *iterator_;
}

const Tile* TilingSetRasterQueueRequired::Top() const {
  DCHECK(!IsEmpty());
  return *iterator_;
}

bool TilingSetRasterQueueRequired::IsTileRequired(const Tile* tile) const {
  return (type_ == RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION &&
          tile->required_for_activation()) ||
         (type_ == RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW &&
          tile->required_for_draw());
}

TilingSetRasterQueueRequired::TilingIterator::TilingIterator()
    : tiling_(nullptr), current_tile_(nullptr) {
}

TilingSetRasterQueueRequired::TilingIterator::TilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data,
    const gfx::Rect& rect)
    : tiling_(tiling), tiling_data_(tiling_data), current_tile_(nullptr) {
  visible_iterator_ =
      TilingData::Iterator(tiling_data_, rect, false /* include_borders */);
  if (!visible_iterator_)
    return;

  current_tile_ =
      tiling_->TileAt(visible_iterator_.index_x(), visible_iterator_.index_y());

  // If this is a valid tile, return it. Note that we have to use a tiling check
  // for occlusion, since the tile's internal state has not yet been updated.
  if (current_tile_ && current_tile_->NeedsRaster() &&
      !tiling_->IsTileOccluded(current_tile_)) {
    tiling_->UpdateTileAndTwinPriority(current_tile_);
    return;
  }
  ++(*this);
}

TilingSetRasterQueueRequired::TilingIterator::~TilingIterator() {
}

TilingSetRasterQueueRequired::TilingIterator&
    TilingSetRasterQueueRequired::TilingIterator::
    operator++() {
  while (true) {
    ++visible_iterator_;
    if (!visible_iterator_) {
      current_tile_ = nullptr;
      return *this;
    }
    std::pair<int, int> next_index = visible_iterator_.index();
    current_tile_ = tiling_->TileAt(next_index.first, next_index.second);
    // If the tile doesn't exist or if it exists but doesn't need raster work,
    // we can move on to the next tile.
    if (!current_tile_ || !current_tile_->NeedsRaster())
      continue;

    // If the tile is occluded, we also can skip it. Note that we use the tiling
    // check for occlusion, since tile's internal state has not yet been updated
    // (by UpdateTileAndTwinPriority). The tiling check does not rely on tile's
    // internal state (it is, in fact, used to determine the tile's state).
    if (tiling_->IsTileOccluded(current_tile_))
      continue;

    // If we get here, that means we have a valid tile that needs raster and is
    // in the NOW bin, which means that it can be required.
    break;
  }

  if (current_tile_)
    tiling_->UpdateTileAndTwinPriority(current_tile_);
  return *this;
}

}  // namespace cc

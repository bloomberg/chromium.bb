// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_tile_set.h"

#include <algorithm>

#include "cc/resources/managed_tile_state.h"
#include "cc/resources/tile.h"

namespace cc {

class BinComparator {
 public:
  bool operator()(const scoped_refptr<Tile>& a,
                  const scoped_refptr<Tile>& b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();

    if (ams.bin[LOW_PRIORITY_BIN] != bms.bin[LOW_PRIORITY_BIN])
      return ams.bin[LOW_PRIORITY_BIN] < bms.bin[LOW_PRIORITY_BIN];

    if (ams.required_for_activation != bms.required_for_activation)
      return ams.required_for_activation;

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    if (ams.time_to_needed_in_seconds !=  bms.time_to_needed_in_seconds)
      return ams.time_to_needed_in_seconds < bms.time_to_needed_in_seconds;

    if (ams.distance_to_visible_in_pixels !=
        bms.distance_to_visible_in_pixels) {
      return ams.distance_to_visible_in_pixels <
             bms.distance_to_visible_in_pixels;
    }

    gfx::Rect a_rect = a->content_rect();
    gfx::Rect b_rect = b->content_rect();
    if (a_rect.y() != b_rect.y())
      return a_rect.y() < b_rect.y();
    return a_rect.x() < b_rect.x();
  }
};

namespace {

typedef std::vector<scoped_refptr<Tile> > TileVector;

void SortBinTiles(ManagedTileBin bin, TileVector* tiles) {
  switch (bin) {
    case NOW_AND_READY_TO_DRAW_BIN:
      break;
    case NOW_BIN:
    case SOON_BIN:
    case EVENTUALLY_AND_ACTIVE_BIN:
    case EVENTUALLY_BIN:
    case NEVER_AND_ACTIVE_BIN:
    case NEVER_BIN:
      std::sort(tiles->begin(), tiles->end(), BinComparator());
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

PrioritizedTileSet::PrioritizedTileSet() {}

PrioritizedTileSet::~PrioritizedTileSet() {}

void PrioritizedTileSet::InsertTile(Tile* tile, ManagedTileBin bin) {
  tiles_[bin].push_back(make_scoped_refptr(tile));
}

void PrioritizedTileSet::Clear() {
  for (int bin = 0; bin < NUM_BINS; ++bin)
    tiles_[bin].clear();
}

void PrioritizedTileSet::Sort() {
  for (int bin = 0; bin < NUM_BINS; ++bin)
    SortBinTiles(static_cast<ManagedTileBin>(bin), &tiles_[bin]);
}

PrioritizedTileSet::PriorityIterator::PriorityIterator(
    PrioritizedTileSet* tile_set)
    : tile_set_(tile_set),
      current_bin_(NOW_AND_READY_TO_DRAW_BIN),
      iterator_(tile_set->tiles_[current_bin_].begin()) {
  if (iterator_ == tile_set_->tiles_[current_bin_].end())
    AdvanceList();
}

PrioritizedTileSet::PriorityIterator::~PriorityIterator() {}

PrioritizedTileSet::PriorityIterator&
PrioritizedTileSet::PriorityIterator::operator++() {
  // We can't increment past the end of the tiles.
  DCHECK(iterator_ != tile_set_->tiles_[current_bin_].end());

  ++iterator_;
  if (iterator_ == tile_set_->tiles_[current_bin_].end())
    AdvanceList();
  return *this;
}

Tile* PrioritizedTileSet::PriorityIterator::operator*() {
  DCHECK(iterator_ != tile_set_->tiles_[current_bin_].end());
  return iterator_->get();
}

void PrioritizedTileSet::PriorityIterator::AdvanceList() {
  DCHECK(iterator_ == tile_set_->tiles_[current_bin_].end());

  while (current_bin_ != NEVER_BIN) {
    current_bin_ = static_cast<ManagedTileBin>(current_bin_ + 1);
    iterator_ = tile_set_->tiles_[current_bin_].begin();
    if (iterator_ != tile_set_->tiles_[current_bin_].end())
      break;
  }
}

}  // namespace cc

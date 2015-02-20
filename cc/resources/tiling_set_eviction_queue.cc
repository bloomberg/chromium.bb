// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/resources/tiling_set_eviction_queue.h"

namespace cc {

TilingSetEvictionQueue::TilingSetEvictionQueue(
    PictureLayerTilingSet* tiling_set,
    TreePriority tree_priority,
    bool skip_shared_out_of_order_tiles)
    : tiling_set_(tiling_set),
      tree_(tiling_set->client()->GetTree()),
      tree_priority_(tree_priority),
      skip_all_shared_tiles_(
          skip_shared_out_of_order_tiles &&
          tree_priority == (tree_ == ACTIVE_TREE ? NEW_CONTENT_TAKES_PRIORITY
                                                 : SMOOTHNESS_TAKES_PRIORITY)),
      skip_shared_out_of_order_tiles_(skip_shared_out_of_order_tiles),
      processing_soon_border_rect_(false),
      processing_tiling_with_required_for_activation_tiles_(false),
      tiling_index_with_required_for_activation_tiles_(0u),
      current_priority_bin_(TilePriority::EVENTUALLY),
      current_tiling_index_(0u),
      current_tiling_range_type_(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES),
      current_eviction_tile_(nullptr) {
  // Early out if the layer has no tilings.
  if (!tiling_set_->num_tilings())
    return;

  tiling_index_with_required_for_activation_tiles_ =
      TilingIndexWithRequiredForActivationTiles();

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

bool TilingSetEvictionQueue::AdvanceToNextEvictionTile() {
  // Advance to the next eviction tile within the current priority bin and
  // tiling. This is done while advancing to a new tiling and while popping
  // the current tile.

  bool required_for_activation =
      processing_tiling_with_required_for_activation_tiles_;

  for (;;) {
    while (spiral_iterator_) {
      std::pair<int, int> next_index = spiral_iterator_.index();
      Tile* tile = current_tiling_->TileAt(next_index.first, next_index.second);
      ++spiral_iterator_;
      if (!tile || !tile->HasResource())
        continue;
      if (skip_all_shared_tiles_ && tile->is_shared())
        continue;
      current_tiling_->UpdateTileAndTwinPriority(tile);
      if (skip_shared_out_of_order_tiles_ && IsSharedOutOfOrderTile(tile))
        continue;
      if (tile->required_for_activation() != required_for_activation)
        continue;
      current_eviction_tile_ = tile;
      return true;
    }
    if (processing_soon_border_rect_) {
      // Advance from soon border rect to skewport rect.
      processing_soon_border_rect_ = false;
      if (current_tiling_->has_skewport_rect_tiles_) {
        spiral_iterator_ = TilingData::ReverseSpiralDifferenceIterator(
            &current_tiling_->tiling_data_,
            current_tiling_->current_skewport_rect_,
            current_tiling_->current_visible_rect_,
            current_tiling_->current_visible_rect_);
        continue;
      }
    }
    break;
  }

  TilePriority::PriorityBin max_tile_priority_bin =
      current_tiling_->client_->GetMaxTilePriorityBin();
  while (visible_iterator_) {
    std::pair<int, int> next_index = visible_iterator_.index();
    Tile* tile = current_tiling_->TileAt(next_index.first, next_index.second);
    ++visible_iterator_;
    if (!tile || !tile->HasResource())
      continue;
    if (skip_all_shared_tiles_ && tile->is_shared())
      continue;
    // If the max tile priority is not NOW, updated priorities for tiles
    // returned by the visible iterator will not have NOW (but EVENTUALLY)
    // priority bin and cannot therefore be required for activation tiles nor
    // occluded NOW tiles in the current tiling.
    if (max_tile_priority_bin <= TilePriority::NOW) {
      // If the current tiling is a pending tree tiling, required for
      // activation tiles can be detected without updating tile priorities.
      if (tree_ == PENDING_TREE &&
          current_tiling_->IsTileRequiredForActivationIfVisible(tile) !=
              required_for_activation) {
        continue;
      }
      // Unoccluded NOW tiles should be evicted (and thus returned) only after
      // all occluded NOW tiles.
      if (!current_tiling_->IsTileOccluded(tile)) {
        unoccluded_now_tiles_.push_back(tile);
        continue;
      }
    }
    current_tiling_->UpdateTileAndTwinPriority(tile);
    if (skip_shared_out_of_order_tiles_ && IsSharedOutOfOrderTile(tile))
      continue;
    if (tile->required_for_activation() != required_for_activation)
      continue;
    current_eviction_tile_ = tile;
    return true;
  }

  while (!unoccluded_now_tiles_.empty()) {
    // All (unoccluded) NOW tiles have the same priority bin (NOW) and the same
    // distance to visible (0.0), so it does not matter that tiles are popped
    // in reversed (FILO) order.
    Tile* tile = unoccluded_now_tiles_.back();
    unoccluded_now_tiles_.pop_back();
    DCHECK(tile);
    if (!tile->HasResource())
      continue;
    current_tiling_->UpdateTileAndTwinPriority(tile);
    if (skip_shared_out_of_order_tiles_ && IsSharedOutOfOrderTile(tile))
      continue;
    if (tile->required_for_activation() != required_for_activation)
      continue;
    current_eviction_tile_ = tile;
    return true;
  }

  current_eviction_tile_ = nullptr;
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextPriorityBin() {
  // Advance to the next priority bin. This is done only after all tiling range
  // types (including the required for activation tiling) within the previous
  // priority bin have been gone through.
  DCHECK_EQ(current_tiling_range_type_, PictureLayerTilingSet::HIGH_RES);

  switch (current_priority_bin_) {
    case TilePriority::EVENTUALLY:
      current_priority_bin_ = TilePriority::SOON;
      return true;
    case TilePriority::SOON:
      current_priority_bin_ = TilePriority::NOW;
      return true;
    case TilePriority::NOW:
      return false;
  }
  NOTREACHED();
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextTilingRangeType() {
  // Advance to the next tiling range type within the current priority bin, to
  // the required for activation tiling range type within the current priority
  // bin or to the first tiling range type within the next priority bin. This
  // is done only after all tilings within the previous tiling range type have
  // been gone through.
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
      // Process required for activation tiles (unless that has already been
      // done for the current priority bin) if there is a tiling with required
      // for activation tiles and that tiling may have required for activation
      // tiles having the current priority bin (in the pending tree only NOW
      // tiles may be required for activation).
      if (!processing_tiling_with_required_for_activation_tiles_ &&
          tiling_index_with_required_for_activation_tiles_ <
              tiling_set_->num_tilings() &&
          (current_priority_bin_ == TilePriority::NOW ||
           tree_ == ACTIVE_TREE)) {
        processing_tiling_with_required_for_activation_tiles_ = true;
        return true;
      }
      processing_tiling_with_required_for_activation_tiles_ = false;

      if (!AdvanceToNextPriorityBin())
        return false;

      current_tiling_range_type_ = PictureLayerTilingSet::HIGHER_THAN_HIGH_RES;
      return true;
  }
  NOTREACHED();
  return false;
}

bool TilingSetEvictionQueue::AdvanceToNextValidTiling() {
  // Advance to the next tiling within current tiling range type or to
  // the first tiling within the next tiling range type or priority bin until
  // the next eviction tile is found. This is done only after all eviction
  // tiles within the previous tiling within the current priority bin and
  // tiling range type have been gone through.
  DCHECK(!current_eviction_tile_);
  DCHECK_NE(current_tiling_index_, CurrentTilingRange().end);

  for (;;) {
    ++current_tiling_index_;
    while (current_tiling_index_ == CurrentTilingRange().end) {
      if (!AdvanceToNextTilingRangeType())
        return false;
      current_tiling_index_ = CurrentTilingRange().start;
    }
    current_tiling_ = tiling_set_->tiling_at(CurrentTilingIndex());

    switch (current_priority_bin_) {
      case TilePriority::EVENTUALLY:
        if (current_tiling_->has_eventually_rect_tiles_) {
          spiral_iterator_ = TilingData::ReverseSpiralDifferenceIterator(
              &current_tiling_->tiling_data_,
              current_tiling_->current_eventually_rect_,
              current_tiling_->current_skewport_rect_,
              current_tiling_->current_soon_border_rect_);
          if (AdvanceToNextEvictionTile())
            return true;
        }
        break;
      case TilePriority::SOON:
        if (current_tiling_->has_skewport_rect_tiles_ ||
            current_tiling_->has_soon_border_rect_tiles_) {
          processing_soon_border_rect_ = true;
          if (current_tiling_->has_soon_border_rect_tiles_)
            spiral_iterator_ = TilingData::ReverseSpiralDifferenceIterator(
                &current_tiling_->tiling_data_,
                current_tiling_->current_soon_border_rect_,
                current_tiling_->current_skewport_rect_,
                current_tiling_->current_visible_rect_);
          if (AdvanceToNextEvictionTile())
            return true;
        }
        break;
      case TilePriority::NOW:
        if (current_tiling_->has_visible_rect_tiles_) {
          visible_iterator_ =
              TilingData::Iterator(&current_tiling_->tiling_data_,
                                   current_tiling_->current_visible_rect_,
                                   false /* include_borders */);
          if (AdvanceToNextEvictionTile())
            return true;
        }
        break;
    }
  }
}

PictureLayerTilingSet::TilingRange
TilingSetEvictionQueue::CurrentTilingRange() const {
  if (processing_tiling_with_required_for_activation_tiles_)
    return PictureLayerTilingSet::TilingRange(
        tiling_index_with_required_for_activation_tiles_,
        tiling_index_with_required_for_activation_tiles_ + 1);
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

bool TilingSetEvictionQueue::IsSharedOutOfOrderTile(const Tile* tile) const {
  if (!tile->is_shared())
    return false;

  switch (tree_priority_) {
    case SMOOTHNESS_TAKES_PRIORITY:
      DCHECK_EQ(ACTIVE_TREE, tree_);
      return false;
    case NEW_CONTENT_TAKES_PRIORITY:
      DCHECK_EQ(PENDING_TREE, tree_);
      return false;
    case SAME_PRIORITY_FOR_BOTH_TREES:
      break;
  }

  // The priority for tile priority of a shared tile will be a combined
  // priority thus return shared tiles from a higher priority tree as
  // it is out of order for a lower priority tree.
  WhichTree twin_tree = tree_ == ACTIVE_TREE ? PENDING_TREE : ACTIVE_TREE;
  const TilePriority& priority = tile->priority(tree_);
  const TilePriority& twin_priority = tile->priority(twin_tree);
  if (priority.priority_bin != twin_priority.priority_bin)
    return priority.priority_bin > twin_priority.priority_bin;
  const bool occluded = tile->is_occluded(tree_);
  const bool twin_occluded = tile->is_occluded(twin_tree);
  if (occluded != twin_occluded)
    return occluded;
  if (priority.distance_to_visible != twin_priority.distance_to_visible)
    return priority.distance_to_visible > twin_priority.distance_to_visible;

  // If priorities are the same, it does not matter which tree returns
  // the tile. Let's pick the pending tree.
  return tree_ != PENDING_TREE;
}

size_t TilingSetEvictionQueue::TilingIndexWithRequiredForActivationTiles()
    const {
  // Returns the tiling index of the tiling with requuired for activation tiles.
  // If no such tiling exists, returns the past-the-last index (num_tilings).
  size_t num_tilings = tiling_set_->num_tilings();

  if (tree_ == PENDING_TREE) {
    // For the pending tree, the tiling with required for activation tiles is
    // the high res one.
    PictureLayerTilingSet::TilingRange high_res_tiling_range =
        tiling_set_->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
    if (high_res_tiling_range.start != high_res_tiling_range.end)
      return high_res_tiling_range.start;
  } else {
    DCHECK_EQ(ACTIVE_TREE, tree_);
    // Only pending tree tiles can be required for activation. They can appear
    // also in the active tree only if they are shared. If we skip all shared
    // tiles, there is no need to find them as they will not be returned.
    if (skip_all_shared_tiles_)
      return num_tilings;

    // For the active tree, the tiling with required for activation tiles is
    // the one whose twin tiling is the high res pending tiling.
    for (size_t i = 0; i < num_tilings; ++i) {
      const PictureLayerTiling* tiling = tiling_set_->tiling_at(i);
      const PictureLayerTiling* pending_tiling =
          tiling_set_->client()->GetPendingOrActiveTwinTiling(tiling);
      if (pending_tiling && pending_tiling->resolution() == HIGH_RESOLUTION)
        return i;
    }
  }

  return num_tilings;
}

}  // namespace cc

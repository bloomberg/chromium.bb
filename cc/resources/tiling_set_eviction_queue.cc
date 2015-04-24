// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/resources/tiling_set_eviction_queue.h"

namespace cc {

TilingSetEvictionQueue::TilingSetEvictionQueue(
    PictureLayerTilingSet* tiling_set,
    bool skip_shared_out_of_order_tiles)
    : tree_(tiling_set->client()->GetTree()),
      skip_shared_out_of_order_tiles_(skip_shared_out_of_order_tiles),
      phase_(EVENTUALLY_RECT),
      current_tile_(nullptr) {
  // Early out if the layer has no tilings.
  if (!tiling_set->num_tilings())
    return;
  GenerateTilingOrder(tiling_set);
  eventually_iterator_ = EventuallyTilingIterator(
      &tilings_, tree_, skip_shared_out_of_order_tiles_);
  if (eventually_iterator_.done()) {
    AdvancePhase();
    return;
  }
  current_tile_ = *eventually_iterator_;
}

TilingSetEvictionQueue::~TilingSetEvictionQueue() {
}

void TilingSetEvictionQueue::GenerateTilingOrder(
    PictureLayerTilingSet* tiling_set) {
  tilings_.reserve(tiling_set->num_tilings());
  // Generate all of the tilings in the order described in the header comment
  // for this class.
  PictureLayerTilingSet::TilingRange range =
      tiling_set->GetTilingRange(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  for (int i = range.start; i < range.end; ++i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  for (int i = range.end - 1; i >= range.start; --i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  for (int i = range.end - 1; i >= range.start; --i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::LOW_RES);
  for (int i = range.start; i < range.end; ++i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  for (int i = range.start; i < range.end; ++i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }
  DCHECK_GE(tiling_set->num_tilings(), tilings_.size());
}

void TilingSetEvictionQueue::AdvancePhase() {
  current_tile_ = nullptr;
  while (!current_tile_ &&
         phase_ != VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED) {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case EVENTUALLY_RECT:
        NOTREACHED();
        break;
      case SOON_BORDER_RECT:
        soon_iterator_ = SoonBorderTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_);
        if (!soon_iterator_.done())
          current_tile_ = *soon_iterator_;
        break;
      case SKEWPORT_RECT:
        skewport_iterator_ = SkewportTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_);
        if (!skewport_iterator_.done())
          current_tile_ = *skewport_iterator_;
        break;
      case PENDING_VISIBLE_RECT:
        pending_visible_iterator_ = PendingVisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            false /* return required for activation tiles */);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
      case PENDING_VISIBLE_RECT_REQUIRED_FOR_ACTIVATION:
        pending_visible_iterator_ = PendingVisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            true /* return required for activation tiles */);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
      case VISIBLE_RECT_OCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            true /* return occluded tiles */,
            false /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_UNOCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            false /* return occluded tiles */,
            false /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_OCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            true /* return occluded tiles */,
            true /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, skip_shared_out_of_order_tiles_,
            false /* return occluded tiles */,
            true /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
    }
  }
}

bool TilingSetEvictionQueue::IsEmpty() const {
  return !current_tile_;
}

Tile* TilingSetEvictionQueue::Top() {
  DCHECK(!IsEmpty());
  return current_tile_;
}

const Tile* TilingSetEvictionQueue::Top() const {
  DCHECK(!IsEmpty());
  return current_tile_;
}

void TilingSetEvictionQueue::Pop() {
  DCHECK(!IsEmpty());
  current_tile_ = nullptr;
  switch (phase_) {
    case EVENTUALLY_RECT:
      ++eventually_iterator_;
      if (!eventually_iterator_.done())
        current_tile_ = *eventually_iterator_;
      break;
    case SOON_BORDER_RECT:
      ++soon_iterator_;
      if (!soon_iterator_.done())
        current_tile_ = *soon_iterator_;
      break;
    case SKEWPORT_RECT:
      ++skewport_iterator_;
      if (!skewport_iterator_.done())
        current_tile_ = *skewport_iterator_;
      break;
    case PENDING_VISIBLE_RECT:
    case PENDING_VISIBLE_RECT_REQUIRED_FOR_ACTIVATION:
      ++pending_visible_iterator_;
      if (!pending_visible_iterator_.done())
        current_tile_ = *pending_visible_iterator_;
      break;
    case VISIBLE_RECT_OCCLUDED:
    case VISIBLE_RECT_UNOCCLUDED:
    case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_OCCLUDED:
    case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED:
      ++visible_iterator_;
      if (!visible_iterator_.done())
        current_tile_ = *visible_iterator_;
      break;
  }
  if (!current_tile_)
    AdvancePhase();
}

// EvictionRectIterator
TilingSetEvictionQueue::EvictionRectIterator::EvictionRectIterator()
    : tile_(nullptr),
      tilings_(nullptr),
      tree_(ACTIVE_TREE),
      skip_shared_out_of_order_tiles_(false),
      tiling_index_(0) {
}

TilingSetEvictionQueue::EvictionRectIterator::EvictionRectIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool skip_shared_out_of_order_tiles,
    bool skip_pending_visible_rect)
    : tile_(nullptr),
      tilings_(tilings),
      tree_(tree),
      skip_shared_out_of_order_tiles_(skip_shared_out_of_order_tiles),
      skip_pending_visible_rect_(skip_pending_visible_rect),
      tiling_index_(0) {
}

template <typename TilingIteratorType>
bool TilingSetEvictionQueue::EvictionRectIterator::AdvanceToNextTile(
    TilingIteratorType* iterator) {
  bool found_tile = false;
  while (!found_tile) {
    ++(*iterator);
    if (!(*iterator)) {
      tile_ = nullptr;
      break;
    }
    found_tile = GetFirstTileAndCheckIfValid(iterator);
  }
  return found_tile;
}

template <typename TilingIteratorType>
bool TilingSetEvictionQueue::EvictionRectIterator::GetFirstTileAndCheckIfValid(
    TilingIteratorType* iterator) {
  PictureLayerTiling* tiling = (*tilings_)[tiling_index_];
  tile_ = tiling->TileAt(iterator->index_x(), iterator->index_y());
  // If there's nothing to evict, return false.
  if (!tile_ || !tile_->HasResource())
    return false;
  if (skip_pending_visible_rect_ &&
      tiling->pending_visible_rect().Intersects(tile_->content_rect())) {
    return false;
  }
  (*tilings_)[tiling_index_]->UpdateTileAndTwinPriority(tile_);
  // In other cases, the tile we got is a viable candidate, return true.
  return true;
}

// EventuallyTilingIterator
TilingSetEvictionQueue::EventuallyTilingIterator::EventuallyTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool skip_shared_out_of_order_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           skip_shared_out_of_order_tiles,
                           true /* skip_pending_visible_rect */) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_eventually_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_eventually_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_soon_border_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::EventuallyTilingIterator&
    TilingSetEvictionQueue::EventuallyTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_eventually_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_eventually_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_soon_border_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// SoonBorderTilingIterator
TilingSetEvictionQueue::SoonBorderTilingIterator::SoonBorderTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool skip_shared_out_of_order_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           skip_shared_out_of_order_tiles,
                           true /* skip_pending_visible_rect */) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_soon_border_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_soon_border_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::SoonBorderTilingIterator&
    TilingSetEvictionQueue::SoonBorderTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_soon_border_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_soon_border_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// SkewportTilingIterator
TilingSetEvictionQueue::SkewportTilingIterator::SkewportTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool skip_shared_out_of_order_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           skip_shared_out_of_order_tiles,
                           true /* skip_pending_visible_rect */) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_skewport_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::SkewportTilingIterator&
    TilingSetEvictionQueue::SkewportTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_skewport_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// PendingVisibleIterator
TilingSetEvictionQueue::PendingVisibleTilingIterator::
    PendingVisibleTilingIterator(std::vector<PictureLayerTiling*>* tilings,
                                 WhichTree tree,
                                 bool skip_shared_out_of_order_tiles,
                                 bool return_required_for_activation_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           skip_shared_out_of_order_tiles,
                           false /* skip_pending_visible_rect */),
      return_required_for_activation_tiles_(
          return_required_for_activation_tiles) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    iterator_ = TilingData::DifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->pending_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (!TileMatchesRequiredFlags(tile_)) {
    ++(*this);
    return;
  }
}

TilingSetEvictionQueue::PendingVisibleTilingIterator&
    TilingSetEvictionQueue::PendingVisibleTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (found_tile && !TileMatchesRequiredFlags(tile_))
    found_tile = AdvanceToNextTile(&iterator_);

  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    iterator_ = TilingData::DifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->pending_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
    while (found_tile && !TileMatchesRequiredFlags(tile_))
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

bool TilingSetEvictionQueue::PendingVisibleTilingIterator::
    TileMatchesRequiredFlags(const Tile* tile) const {
  bool activation_flag_matches =
      tile->required_for_activation() == return_required_for_activation_tiles_;
  return activation_flag_matches;
}

// VisibleTilingIterator
TilingSetEvictionQueue::VisibleTilingIterator::VisibleTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool skip_shared_out_of_order_tiles,
    bool return_occluded_tiles,
    bool return_required_for_activation_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           skip_shared_out_of_order_tiles,
                           false /* skip_pending_visible_rect */),
      return_occluded_tiles_(return_occluded_tiles),
      return_required_for_activation_tiles_(
          return_required_for_activation_tiles) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_visible_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::Iterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_visible_rect(), false);
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (!TileMatchesRequiredFlags(tile_)) {
    ++(*this);
    return;
  }
}

TilingSetEvictionQueue::VisibleTilingIterator&
    TilingSetEvictionQueue::VisibleTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (found_tile && !TileMatchesRequiredFlags(tile_))
    found_tile = AdvanceToNextTile(&iterator_);

  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_visible_rect_tiles())
      continue;
    iterator_ = TilingData::Iterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_visible_rect(), false);
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
    while (found_tile && !TileMatchesRequiredFlags(tile_))
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

bool TilingSetEvictionQueue::VisibleTilingIterator::TileMatchesRequiredFlags(
    const Tile* tile) const {
  bool activation_flag_matches =
      tile->required_for_activation() == return_required_for_activation_tiles_;
  bool occluded_flag_matches = tile->is_occluded() == return_occluded_tiles_;
  return activation_flag_matches && occluded_flag_matches;
}

}  // namespace cc

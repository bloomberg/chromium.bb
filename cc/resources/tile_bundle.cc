// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_bundle.h"

#include <algorithm>

#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_manager.h"

namespace cc {

TileBundle::Id TileBundle::s_next_id_ = 0;

TileBundle::TileBundle(TileManager* tile_manager,
                       int offset_x,
                       int offset_y,
                       int width,
                       int height)
    : RefCountedManaged<TileBundle>(tile_manager),
      tile_manager_(tile_manager),
      index_offset_x_(offset_x),
      index_offset_y_(offset_y),
      index_count_x_(width),
      index_count_y_(height),
      conservative_tile_count_(0),
      needs_tile_swap_(false),
      id_(s_next_id_++) {
  tiles_[ACTIVE_TREE].resize(index_count_y_);
  for (int i = 0; i < index_count_y_; ++i)
    tiles_[ACTIVE_TREE][i].resize(index_count_x_);

  tiles_[PENDING_TREE].resize(index_count_y_);
  for (int i = 0; i < index_count_y_; ++i)
    tiles_[PENDING_TREE][i].resize(index_count_x_);
}

TileBundle::~TileBundle() {}

Tile* TileBundle::TileAt(WhichTree tree, int index_x, int index_y) {
  DCHECK(!needs_tile_swap_);

  UpdateToLocalIndex(&index_x, &index_y);
  return TileAtLocalIndex(tree, index_x, index_y);
}

Tile* TileBundle::TileAtLocalIndex(WhichTree tree, int index_x, int index_y) {
  return tiles_[tree][index_y][index_x].get();
}

void TileBundle::SetPriority(WhichTree tree, const TilePriority& priority) {
  DCHECK(!needs_tile_swap_);

  if (priority_[tree] == priority)
    return;

  priority_[tree] = priority;
  tile_manager_->DidChangeTileBundlePriority(this);
}

TilePriority TileBundle::GetPriority(WhichTree tree) const {
  return priority_[tree];
}

bool TileBundle::RemoveTileAt(WhichTree tree, int index_x, int index_y) {
  DCHECK(!needs_tile_swap_);

  UpdateToLocalIndex(&index_x, &index_y);
  bool removed = !!tiles_[tree][index_y][index_x];
  tiles_[tree][index_y][index_x] = NULL;
  if (removed)
    --conservative_tile_count_;
  return removed;
}

void TileBundle::AddTileAt(WhichTree tree,
                           int index_x,
                           int index_y,
                           scoped_refptr<Tile> tile) {
  DCHECK(!needs_tile_swap_);

  UpdateToLocalIndex(&index_x, &index_y);
  DCHECK(!tiles_[tree][index_y][index_x]);
  tiles_[tree][index_y][index_x] = tile;
  ++conservative_tile_count_;
}

void TileBundle::DidBecomeRecycled() {
  priority_[ACTIVE_TREE] = TilePriority();
  needs_tile_swap_ = !needs_tile_swap_;
}

void TileBundle::DidBecomeActive() {
  priority_[ACTIVE_TREE] = priority_[PENDING_TREE];
  priority_[PENDING_TREE] = TilePriority();
  tiles_[ACTIVE_TREE].swap(tiles_[PENDING_TREE]);
  needs_tile_swap_ = false;
}

void TileBundle::UpdateToLocalIndex(int* index_x, int* index_y) {
  DCHECK(index_x);
  DCHECK(index_y);

  *index_x -= index_offset_x_;
  *index_y -= index_offset_y_;

  DCHECK_GE(*index_x, 0);
  DCHECK_GE(*index_y, 0);
  DCHECK_LT(*index_x, index_count_x_);
  DCHECK_LT(*index_x, index_count_y_);
}

void TileBundle::SwapTilesIfRequired() {
  if (!needs_tile_swap_)
    return;

  std::swap(priority_[ACTIVE_TREE], priority_[PENDING_TREE]);
  tiles_[ACTIVE_TREE].swap(tiles_[PENDING_TREE]);
  needs_tile_swap_ = false;
}

TileBundle::Iterator::Iterator(TileBundle* bundle)
    : bundle_(bundle),
      current_tile_(NULL),
      index_x_(0),
      index_y_(0),
      current_tree_(ACTIVE_TREE),
      min_tree_(ACTIVE_TREE),
      max_tree_(PENDING_TREE) {
  if (!InitializeNewTile())
    ++(*this);
}

TileBundle::Iterator::Iterator(TileBundle* bundle, WhichTree tree)
    : bundle_(bundle),
      current_tile_(NULL),
      index_x_(0),
      index_y_(0),
      current_tree_(tree),
      min_tree_(tree),
      max_tree_(tree) {
  if (!InitializeNewTile())
    ++(*this);
}

TileBundle::Iterator::~Iterator() {}

TileBundle::Iterator& TileBundle::Iterator::operator++() {
  do {
    current_tree_ = static_cast<WhichTree>(current_tree_ + 1);
    if (current_tree_ > max_tree_) {
      current_tree_ = min_tree_;
      ++index_x_;
      if (index_x_ >= bundle_->index_count_x_) {
        index_x_ = 0;
        ++index_y_;
        if (index_y_ >= bundle_->index_count_y_)
          break;
      }
    }
  } while (!InitializeNewTile());

  return *this;
}

bool TileBundle::Iterator::InitializeNewTile() {
  Tile* tile = bundle_->TileAtLocalIndex(current_tree_, index_x_, index_y_);

  // We succeed only if we have a tile and it is different
  // from what we currently have. This is to avoid the iterator
  // returning the same tile twice when it's shared between trees.
  if (!tile || tile == current_tile_)
    return false;

  current_tile_ = tile;
  return true;
}

}  // namespace cc

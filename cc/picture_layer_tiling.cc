// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling.h"

namespace cc {

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Create(
    gfx::Size tile_size) {
  return make_scoped_ptr(new PictureLayerTiling(tile_size));
}

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Clone() const {
  return make_scoped_ptr(new PictureLayerTiling(*this));
}

PictureLayerTiling::PictureLayerTiling(gfx::Size tile_size)
    : client_(NULL),
      tiling_data_(tile_size, gfx::Size(), true) {
}

PictureLayerTiling::~PictureLayerTiling() {
}

const PictureLayerTiling& PictureLayerTiling::operator=(
    const PictureLayerTiling& tiler) {
  tiling_data_ = tiler.tiling_data_;
  tiles_ = tiler.tiles_;
  return *this;
}

void PictureLayerTiling::create_tiles(gfx::Rect rect) {
  for (Iterator iter(this, rect); iter; ++iter) {
    if (*iter)
      continue;
    tiles_[std::make_pair(iter.tile_i_, iter.tile_j_)] =
        client_->CreateTile(this, iter.full_tile_rect());
  }
}

void PictureLayerTiling::set_client(PictureLayerTilingClient* client) {
  client_ = client;
}

Tile* PictureLayerTiling::TileAt(int i, int j) const {
  TileMap::const_iterator iter = tiles_.find(std::make_pair(i, j));
  if (iter == tiles_.end())
    return NULL;
  return iter->second.get();
}

Region PictureLayerTiling::OpaqueRegionInContentRect(
    const gfx::Rect& content_rect) const {
  Region opaque_region;
  // TODO(enne): implement me
  return opaque_region;
}

void PictureLayerTiling::SetBounds(gfx::Size size) {
  tiling_data_.SetTotalSize(size);
  if (size.IsEmpty()) {
    tiles_.clear();
    return;
  }

  // Any tiles outside our new bounds are invalid and should be dropped.
  int right = tiling_data_.TileXIndexFromSrcCoord(size.width() - 1);
  int bottom = tiling_data_.TileYIndexFromSrcCoord(size.height() - 1);
  std::vector<TileMapKey> invalid_tile_keys;
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    if (it->first.first > right || it->first.second > bottom)
      invalid_tile_keys.push_back(it->first);
  }
  for (size_t i = 0; i < invalid_tile_keys.size(); ++i)
    tiles_.erase(invalid_tile_keys[i]);
}

PictureLayerTiling::Iterator::Iterator(PictureLayerTiling* tiling,
                                       gfx::Rect content_rect)
    : tiling_(tiling),
      content_rect_(content_rect),
      current_tile_(NULL),
      tile_i_(0),
      tile_j_(0),
      left_(0),
      top_(0),
      right_(0),
      bottom_(0) {
  DCHECK(tiling_);
  if (content_rect_.IsEmpty())
    return;

  left_ = tiling_->tiling_data_.TileXIndexFromSrcCoord(content_rect.x());
  top_ = tiling_->tiling_data_.TileYIndexFromSrcCoord(content_rect.y());
  right_ = tiling_->tiling_data_.TileXIndexFromSrcCoord(
      content_rect.right() - 1);
  bottom_ = tiling_->tiling_data_.TileYIndexFromSrcCoord(
      content_rect.bottom() - 1);

  tile_i_ = left_;
  tile_j_ = top_;
  current_tile_ = tiling_->TileAt(tile_i_, tile_j_);
}

PictureLayerTiling::Iterator::~Iterator() {
}

PictureLayerTiling::Iterator& PictureLayerTiling::Iterator::operator++() {
  if (!current_tile_)
    return *this;

  do {
    tile_i_++;
    if (tile_i_ > right_) {
      tile_i_ = left_;
      tile_j_++;
      if (tile_j_ > top_)
        current_tile_ = NULL;
        return *this;
    }
  } while (!geometry_rect().IsEmpty());

  current_tile_ = tiling_->TileAt(tile_i_, tile_j_);
  return *this;
}

gfx::Rect PictureLayerTiling::Iterator::geometry_rect() const {
  gfx::Rect geometry_rect = tiling_->tiling_data_.TileBounds(tile_i_, tile_j_);
  geometry_rect.Intersect(content_rect_);
  return geometry_rect;
}

gfx::Rect PictureLayerTiling::Iterator::full_tile_rect() const {
  gfx::Rect tile_rect =
      tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_);
  tile_rect.set_size(texture_size());
  return tile_rect;
}

gfx::Rect PictureLayerTiling::Iterator::texture_rect() const {
  gfx::Rect full_bounds = tiling_->tiling_data_.TileBounds(tile_i_, tile_j_);
  gfx::Rect visible = geometry_rect();
  gfx::Vector2d display_offset = visible.origin() - full_bounds.origin();
  gfx::Vector2d offset = visible.origin() - full_bounds.origin();
  offset += tiling_->tiling_data_.TextureOffset(tile_i_, tile_j_);

  return gfx::Rect(gfx::PointAtOffsetFromOrigin(offset), visible.size());
}

gfx::Rect PictureLayerTiling::Iterator::opaque_rect() const {
  gfx::Rect opaque_rect;
  opaque_rect = current_tile_->opaque_rect();
  opaque_rect.Intersect(content_rect_);
  return opaque_rect;
}

gfx::Size PictureLayerTiling::Iterator::texture_size() const {
  return tiling_->tiling_data_.max_texture_size();
}

bool PictureLayerTiling::Iterator::operator==(const Iterator& other) const {
  return tiling_ == other.tiling_ && current_tile_ == other.current_tile_;
}

bool PictureLayerTiling::Iterator::operator!=(const Iterator& other) const {
  return !(*this == other);
}

}  // namespace cc

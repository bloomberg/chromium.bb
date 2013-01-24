// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiling_data.h"

#include <algorithm>

#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

namespace cc {

static int ComputeNumTiles(int max_texture_size, int total_size, int border_texels) {
  if (max_texture_size - 2 * border_texels <= 0)
    return total_size > 0 && max_texture_size >= total_size ? 1 : 0;

  int num_tiles = std::max(1, 1 + (total_size - 1 - 2 * border_texels) / (max_texture_size - 2 * border_texels));
  return total_size > 0 ? num_tiles : 0;
}

TilingData::TilingData()
    : border_texels_(0) {
  RecomputeNumTiles();
}

TilingData::TilingData(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool hasBorderTexels)
    : max_texture_size_(max_texture_size),
      total_size_(total_size),
      border_texels_(hasBorderTexels ? 1 : 0) {
  RecomputeNumTiles();
}

TilingData::TilingData(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    int border_texels)
    : max_texture_size_(max_texture_size),
      total_size_(total_size),
      border_texels_(border_texels) {
  RecomputeNumTiles();
}

void TilingData::SetTotalSize(gfx::Size total_size) {
  total_size_ = total_size;
  RecomputeNumTiles();
}

void TilingData::SetMaxTextureSize(gfx::Size max_texture_size) {
  max_texture_size_ = max_texture_size;
  RecomputeNumTiles();
}

void TilingData::SetHasBorderTexels(bool has_border_texels) {
  border_texels_ = has_border_texels ? 1 : 0;
  RecomputeNumTiles();
}

void TilingData::SetBorderTexels(int border_texels) {
  border_texels_ = border_texels;
  RecomputeNumTiles();
}

int TilingData::TileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int x = (src_position - border_texels_) /
      (max_texture_size_.width() - 2 * border_texels_);
  return std::min(std::max(x, 0), num_tiles_x_ - 1);
}

int TilingData::TileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int y = (src_position - border_texels_) /
      (max_texture_size_.height() - 2 * border_texels_);
  return std::min(std::max(y, 0), num_tiles_y_ - 1);
}

int TilingData::BorderTileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.width() - 2 * border_texels_;
  int x = (src_position - 2 * border_texels_) / inner_tile_size;
  return std::min(std::max(x, 0), num_tiles_x_ - 1);
}

int TilingData::BorderTileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.height() - 2 * border_texels_;
  int y = (src_position - 2 * border_texels_) / inner_tile_size;
  return std::min(std::max(y, 0), num_tiles_y_ - 1);
}

gfx::Rect TilingData::TileBounds(int i, int j) const {
  AssertTile(i, j);
  int x = TilePositionX(i);
  int y = TilePositionY(j);
  int width = TileSizeX(i);
  int height = TileSizeY(j);
  DCHECK_GE(x, 0);
  DCHECK_GE(y, 0);
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_LE(x, total_size_.width());
  DCHECK_LE(y, total_size_.height());
  return gfx::Rect(x, y, width, height);
}

gfx::Rect TilingData::TileBoundsWithBorder(int i, int j) const {
  gfx::Rect bounds = TileBounds(i, j);

  if (border_texels_) {
    int x1 = bounds.x();
    int x2 = bounds.right();
    int y1 = bounds.y();
    int y2 = bounds.bottom();

    if (i > 0)
      x1-= border_texels_;
    if (i < (num_tiles_x_ - 1))
      x2+= border_texels_;
    if (j > 0)
      y1-= border_texels_;
    if (j < (num_tiles_y_ - 1))
      y2+= border_texels_;

    bounds = gfx::Rect(x1, y1, x2 - x1, y2 - y1);
  }

  return bounds;
}

int TilingData::TilePositionX(int x_index) const {
  DCHECK_GE(x_index, 0);
  DCHECK_LT(x_index, num_tiles_x_);

  int pos = (max_texture_size_.width() - 2 * border_texels_) * x_index;
  if (x_index != 0)
    pos += border_texels_;

  return pos;
}

int TilingData::TilePositionY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  int pos = (max_texture_size_.height() - 2 * border_texels_) * y_index;
  if (y_index != 0)
    pos += border_texels_;

  return pos;
}

int TilingData::TileSizeX(int x_index) const {
  DCHECK_GE(x_index, 0);
  DCHECK_LT(x_index, num_tiles_x_);

  if (!x_index && num_tiles_x_ == 1)
    return total_size_.width();
  if (!x_index && num_tiles_x_ > 1)
    return max_texture_size_.width() - border_texels_;
  if (x_index < num_tiles_x_ - 1)
    return max_texture_size_.width() - 2 * border_texels_;
  if (x_index == num_tiles_x_ - 1)
    return total_size_.width() - TilePositionX(x_index);

  NOTREACHED();
  return 0;
}

int TilingData::TileSizeY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  if (!y_index && num_tiles_y_ == 1)
    return total_size_.height();
  if (!y_index && num_tiles_y_ > 1)
    return max_texture_size_.height() - border_texels_;
  if (y_index < num_tiles_y_ - 1)
    return max_texture_size_.height() - 2 * border_texels_;
  if (y_index == num_tiles_y_ - 1)
    return total_size_.height() - TilePositionY(y_index);

  NOTREACHED();
  return 0;
}

gfx::Vector2d TilingData::TextureOffset(int x_index, int y_index) const {
  int left = (!x_index || num_tiles_x_ == 1) ? 0 : border_texels_;
  int top = (!y_index || num_tiles_y_ == 1) ? 0 : border_texels_;

  return gfx::Vector2d(left, top);
}

void TilingData::RecomputeNumTiles() {
  num_tiles_x_ = ComputeNumTiles(max_texture_size_.width(), total_size_.width(), border_texels_);
  num_tiles_y_ = ComputeNumTiles(max_texture_size_.height(), total_size_.height(), border_texels_);
}

TilingData::Iterator::Iterator(const TilingData* tiling_data, gfx::Rect rect)
    : tiling_data_(tiling_data),
      rect_(gfx::IntersectRects(rect, gfx::Rect(tiling_data_->total_size()))) {
  if (tiling_data_->num_tiles_x() <= 0 || tiling_data_->num_tiles_y() <= 0) {
    done();
    return;
  }

  index_x_ = tiling_data_->BorderTileXIndexFromSrcCoord(rect_.x());
  index_y_ = tiling_data_->BorderTileYIndexFromSrcCoord(rect_.y());

  // Index functions always return valid indices, so explicitly check
  // for non-intersecting rects.
  gfx::Rect new_rect = tiling_data_->TileBoundsWithBorder(index_x_, index_y_);
  if (!new_rect.Intersects(rect_))
    done();
}

TilingData::Iterator& TilingData::Iterator::operator++() {
  if (!*this)
    return *this;

  index_x_++;

  bool new_row = index_x_ >= tiling_data_->num_tiles_x();
  if (!new_row) {
    gfx::Rect new_rect = tiling_data_->TileBoundsWithBorder(index_x_, index_y_);
    new_row = new_rect.x() >= rect_.right();
  }

  if (new_row) {
    index_x_ = tiling_data_->BorderTileXIndexFromSrcCoord(rect_.x());
    index_y_++;

    if (index_y_ >= tiling_data_->num_tiles_y()) {
      done();
    } else {
      gfx::Rect new_rect =
          tiling_data_->TileBoundsWithBorder(index_x_, index_y_);
      if (new_rect.y() >= rect_.bottom())
        done();
    }
  }

  return *this;
}

TilingData::Iterator::operator bool() const {
  return index_x_ != -1 && index_y_ != -1;
}

void TilingData::Iterator::done() {
  index_x_ = -1;
  index_y_ = -1;
}

}  // namespace cc

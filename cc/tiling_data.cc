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

TilingData::TilingData(gfx::Size max_texture_size, gfx::Size total_size, bool hasBorderTexels)
    : max_texture_size_(max_texture_size),
      total_size_(total_size),
      border_texels_(hasBorderTexels ? 1 : 0) {
  RecomputeNumTiles();
}

TilingData::~TilingData() {
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

int TilingData::TileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK(max_texture_size_.width() - 2 * border_texels_);
  int x = (src_position - border_texels_) / (max_texture_size_.width() - 2 * border_texels_);
  return std::min(std::max(x, 0), num_tiles_x_ - 1);
}

int TilingData::TileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK(max_texture_size_.height() - 2 * border_texels_);
  int y = (src_position - border_texels_) / (max_texture_size_.height() - 2 * border_texels_);
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
      x1--;
    if (i < (num_tiles_x_ - 1))
      x2++;
    if (j > 0)
      y1--;
    if (j < (num_tiles_y_ - 1))
      y2++;

    bounds = gfx::Rect(x1, y1, x2 - x1, y2 - y1);
  }

  return bounds;
}

int TilingData::TilePositionX(int x_index) const {
  DCHECK_GE(x_index, 0);
  DCHECK_LT(x_index, num_tiles_x_);

  int pos = 0;
  for (int i = 0; i < x_index; i++)
    pos += TileSizeX(i);

  return pos;
}

int TilingData::TilePositionY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  int pos = 0;
  for (int i = 0; i < y_index; i++)
    pos += TileSizeY(i);

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

}  // namespace cc

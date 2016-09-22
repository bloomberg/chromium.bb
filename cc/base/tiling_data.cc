// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/tiling_data.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

namespace {
// IndexRect which is at left top corner of the positive quadrant.
const IndexRect kNonPositiveQuadrantIndexRect(-1, -1, -1, -1);
}

static int ComputeNumTiles(int max_texture_size,
                           int total_size,
                           int border_texels) {
  if (max_texture_size - 2 * border_texels <= 0)
    return total_size > 0 && max_texture_size >= total_size ? 1 : 0;

  int num_tiles = std::max(1,
                           1 + (total_size - 1 - 2 * border_texels) /
                           (max_texture_size - 2 * border_texels));
  return total_size > 0 ? num_tiles : 0;
}

TilingData::TilingData()
    : border_texels_(0) {
  RecomputeNumTiles();
}

TilingData::TilingData(const gfx::Size& max_texture_size,
                       const gfx::Size& tiling_size,
                       bool has_border_texels)
    : max_texture_size_(max_texture_size),
      tiling_size_(tiling_size),
      border_texels_(has_border_texels ? 1 : 0) {
  RecomputeNumTiles();
}

TilingData::TilingData(const gfx::Size& max_texture_size,
                       const gfx::Size& tiling_size,
                       int border_texels)
    : max_texture_size_(max_texture_size),
      tiling_size_(tiling_size),
      border_texels_(border_texels) {
  RecomputeNumTiles();
}

void TilingData::SetTilingSize(const gfx::Size& tiling_size) {
  tiling_size_ = tiling_size;
  RecomputeNumTiles();
}

void TilingData::SetMaxTextureSize(const gfx::Size& max_texture_size) {
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

int TilingData::FirstBorderTileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.width() - 2 * border_texels_;
  int x = (src_position - 2 * border_texels_) / inner_tile_size;
  return std::min(std::max(x, 0), num_tiles_x_ - 1);
}

int TilingData::FirstBorderTileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.height() - 2 * border_texels_;
  int y = (src_position - 2 * border_texels_) / inner_tile_size;
  return std::min(std::max(y, 0), num_tiles_y_ - 1);
}

int TilingData::LastBorderTileXIndexFromSrcCoord(int src_position) const {
  if (num_tiles_x_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.width() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.width() - 2 * border_texels_;
  int x = src_position / inner_tile_size;
  return std::min(std::max(x, 0), num_tiles_x_ - 1);
}

int TilingData::LastBorderTileYIndexFromSrcCoord(int src_position) const {
  if (num_tiles_y_ <= 1)
    return 0;

  DCHECK_GT(max_texture_size_.height() - 2 * border_texels_, 0);
  int inner_tile_size = max_texture_size_.height() - 2 * border_texels_;
  int y = src_position / inner_tile_size;
  return std::min(std::max(y, 0), num_tiles_y_ - 1);
}

gfx::Rect TilingData::ExpandRectIgnoringBordersToTileBounds(
    const gfx::Rect& rect) const {
  if (rect.IsEmpty() || has_empty_bounds())
    return gfx::Rect();
  if (rect.x() > tiling_size_.width() || rect.y() > tiling_size_.height())
    return gfx::Rect();
  int index_x = TileXIndexFromSrcCoord(rect.x());
  int index_y = TileYIndexFromSrcCoord(rect.y());
  int index_right = TileXIndexFromSrcCoord(rect.right() - 1);
  int index_bottom = TileYIndexFromSrcCoord(rect.bottom() - 1);

  gfx::Rect rect_top_left(TileBounds(index_x, index_y));
  gfx::Rect rect_bottom_right(TileBounds(index_right, index_bottom));

  return gfx::UnionRects(rect_top_left, rect_bottom_right);
}

gfx::Rect TilingData::ExpandRectToTileBounds(const gfx::Rect& rect) const {
  if (rect.IsEmpty() || has_empty_bounds())
    return gfx::Rect();
  if (rect.x() > tiling_size_.width() || rect.y() > tiling_size_.height())
    return gfx::Rect();
  int index_x = FirstBorderTileXIndexFromSrcCoord(rect.x());
  int index_y = FirstBorderTileYIndexFromSrcCoord(rect.y());
  int index_right = LastBorderTileXIndexFromSrcCoord(rect.right() - 1);
  int index_bottom = LastBorderTileYIndexFromSrcCoord(rect.bottom() - 1);

  gfx::Rect rect_top_left(TileBounds(index_x, index_y));
  gfx::Rect rect_bottom_right(TileBounds(index_right, index_bottom));

  return gfx::UnionRects(rect_top_left, rect_bottom_right);
}

gfx::Rect TilingData::TileBounds(int i, int j) const {
  AssertTile(i, j);
  int max_texture_size_x = max_texture_size_.width() - 2 * border_texels_;
  int max_texture_size_y = max_texture_size_.height() - 2 * border_texels_;

  int lo_x = max_texture_size_x * i;
  if (i != 0)
    lo_x += border_texels_;

  int lo_y = max_texture_size_y * j;
  if (j != 0)
    lo_y += border_texels_;

  int hi_x = max_texture_size_x * (i + 1) + border_texels_;
  if (i + 1 == num_tiles_x_)
    hi_x += border_texels_;

  int hi_y = max_texture_size_y * (j + 1) + border_texels_;
  if (j + 1 == num_tiles_y_)
    hi_y += border_texels_;

  hi_x = std::min(hi_x, tiling_size_.width());
  hi_y = std::min(hi_y, tiling_size_.height());

  int x = lo_x;
  int y = lo_y;
  int width = hi_x - lo_x;
  int height = hi_y - lo_y;
  DCHECK_GE(x, 0);
  DCHECK_GE(y, 0);
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_LE(x, tiling_size_.width());
  DCHECK_LE(y, tiling_size_.height());
  return gfx::Rect(x, y, width, height);
}

gfx::Rect TilingData::TileBoundsWithBorder(int i, int j) const {
  AssertTile(i, j);
  int max_texture_size_x = max_texture_size_.width() - 2 * border_texels_;
  int max_texture_size_y = max_texture_size_.height() - 2 * border_texels_;

  int lo_x = max_texture_size_x * i;
  int lo_y = max_texture_size_y * j;

  int hi_x = lo_x + max_texture_size_x + 2 * border_texels_;
  int hi_y = lo_y + max_texture_size_y + 2 * border_texels_;

  hi_x = std::min(hi_x, tiling_size_.width());
  hi_y = std::min(hi_y, tiling_size_.height());

  int x = lo_x;
  int y = lo_y;
  int width = hi_x - lo_x;
  int height = hi_y - lo_y;
  DCHECK_GE(x, 0);
  DCHECK_GE(y, 0);
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_LE(x, tiling_size_.width());
  DCHECK_LE(y, tiling_size_.height());
  return gfx::Rect(x, y, width, height);
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
    return tiling_size_.width();
  if (!x_index && num_tiles_x_ > 1)
    return max_texture_size_.width() - border_texels_;
  if (x_index < num_tiles_x_ - 1)
    return max_texture_size_.width() - 2 * border_texels_;
  if (x_index == num_tiles_x_ - 1)
    return tiling_size_.width() - TilePositionX(x_index);

  NOTREACHED();
  return 0;
}

int TilingData::TileSizeY(int y_index) const {
  DCHECK_GE(y_index, 0);
  DCHECK_LT(y_index, num_tiles_y_);

  if (!y_index && num_tiles_y_ == 1)
    return tiling_size_.height();
  if (!y_index && num_tiles_y_ > 1)
    return max_texture_size_.height() - border_texels_;
  if (y_index < num_tiles_y_ - 1)
    return max_texture_size_.height() - 2 * border_texels_;
  if (y_index == num_tiles_y_ - 1)
    return tiling_size_.height() - TilePositionY(y_index);

  NOTREACHED();
  return 0;
}

gfx::RectF TilingData::TexelExtent(int i, int j) const {
  gfx::RectF result(TileBoundsWithBorder(i, j));
  result.Inset(0.5f, 0.5f);
  return result;
}

gfx::Vector2d TilingData::TextureOffset(int x_index, int y_index) const {
  int left = (!x_index || num_tiles_x_ == 1) ? 0 : border_texels_;
  int top = (!y_index || num_tiles_y_ == 1) ? 0 : border_texels_;

  return gfx::Vector2d(left, top);
}

void TilingData::RecomputeNumTiles() {
  num_tiles_x_ = ComputeNumTiles(
      max_texture_size_.width(), tiling_size_.width(), border_texels_);
  num_tiles_y_ = ComputeNumTiles(
      max_texture_size_.height(), tiling_size_.height(), border_texels_);
}

TilingData::BaseIterator::BaseIterator() : index_x_(-1), index_y_(-1) {
}

TilingData::Iterator::Iterator() : index_rect_(kNonPositiveQuadrantIndexRect) {
  done();
}

TilingData::Iterator::Iterator(const TilingData* tiling_data,
                               const gfx::Rect& consider_rect,
                               bool include_borders)
    : index_rect_(kNonPositiveQuadrantIndexRect) {
  if (tiling_data->num_tiles_x() <= 0 || tiling_data->num_tiles_y() <= 0) {
    done();
    return;
  }

  gfx::Rect tiling_bounds_rect(tiling_data->tiling_size());
  gfx::Rect rect(consider_rect);
  rect.Intersect(tiling_bounds_rect);

  gfx::Rect top_left_tile;
  if (include_borders) {
    index_x_ = tiling_data->FirstBorderTileXIndexFromSrcCoord(rect.x());
    index_y_ = tiling_data->FirstBorderTileYIndexFromSrcCoord(rect.y());
    index_rect_ = IndexRect(
        index_x_,
        tiling_data->LastBorderTileXIndexFromSrcCoord(rect.right() - 1),
        index_y_,
        tiling_data->LastBorderTileYIndexFromSrcCoord(rect.bottom() - 1));
    DCHECK(index_rect_.is_valid());
    top_left_tile = tiling_data->TileBoundsWithBorder(index_x_, index_y_);
  } else {
    index_x_ = tiling_data->TileXIndexFromSrcCoord(rect.x());
    index_y_ = tiling_data->TileYIndexFromSrcCoord(rect.y());
    index_rect_ = IndexRect(
        index_x_, tiling_data->TileXIndexFromSrcCoord(rect.right() - 1),
        index_y_, tiling_data->TileYIndexFromSrcCoord(rect.bottom() - 1));
    DCHECK(index_rect_.is_valid());
    top_left_tile = tiling_data->TileBounds(index_x_, index_y_);
  }

  // Index functions always return valid indices, so explicitly check
  // for non-intersecting rects.
  if (!top_left_tile.Intersects(rect))
    done();
}

TilingData::Iterator& TilingData::Iterator::operator++() {
  if (!*this)
    return *this;

  index_x_++;
  if (index_x_ > index_rect_.right()) {
    index_x_ = index_rect_.left();
    index_y_++;
    if (index_y_ > index_rect_.bottom())
      done();
  }

  return *this;
}

TilingData::BaseDifferenceIterator::BaseDifferenceIterator()
    : consider_index_rect_(kNonPositiveQuadrantIndexRect),
      ignore_index_rect_(kNonPositiveQuadrantIndexRect) {
  done();
}

TilingData::BaseDifferenceIterator::BaseDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect)
    : consider_index_rect_(kNonPositiveQuadrantIndexRect),
      ignore_index_rect_(kNonPositiveQuadrantIndexRect) {
  if (tiling_data->num_tiles_x() <= 0 || tiling_data->num_tiles_y() <= 0) {
    done();
    return;
  }

  gfx::Rect tiling_bounds_rect(tiling_data->tiling_size());
  gfx::Rect consider(consider_rect);
  consider.Intersect(tiling_bounds_rect);

  if (consider.IsEmpty()) {
    done();
    return;
  }

  consider_index_rect_ =
      IndexRect(tiling_data->TileXIndexFromSrcCoord(consider.x()),
                tiling_data->TileXIndexFromSrcCoord(consider.right() - 1),
                tiling_data->TileYIndexFromSrcCoord(consider.y()),
                tiling_data->TileYIndexFromSrcCoord(consider.bottom() - 1));
  DCHECK(consider_index_rect_.is_valid());

  gfx::Rect ignore(ignore_rect);
  ignore.Intersect(tiling_bounds_rect);

  if (!ignore.IsEmpty()) {
    ignore_index_rect_ =
        IndexRect(tiling_data->TileXIndexFromSrcCoord(ignore.x()),
                  tiling_data->TileXIndexFromSrcCoord(ignore.right() - 1),
                  tiling_data->TileYIndexFromSrcCoord(ignore.y()),
                  tiling_data->TileYIndexFromSrcCoord(ignore.bottom() - 1));
    DCHECK(ignore_index_rect_.is_valid());

    // Clamp ignore indices to consider indices.
    ignore_index_rect_.ClampTo(consider_index_rect_);

    // If ignore rect is invalid, reset.
    if (!ignore_index_rect_.is_valid())
      ignore_index_rect_ = kNonPositiveQuadrantIndexRect;

    if (ignore_index_rect_ == consider_index_rect_) {
      consider_index_rect_ = kNonPositiveQuadrantIndexRect;
      done();
      return;
    }
  }
}

bool TilingData::BaseDifferenceIterator::HasConsiderRect() const {
  // Consider indices are either all valid or all equal to -1.
  DCHECK(consider_index_rect_.is_in_positive_quadrant() ||
         consider_index_rect_ == kNonPositiveQuadrantIndexRect);
  return consider_index_rect_.left() != -1;
}

TilingData::DifferenceIterator::DifferenceIterator() {
}

TilingData::DifferenceIterator::DifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  index_x_ = consider_index_rect_.left();
  index_y_ = consider_index_rect_.top();

  if (ignore_index_rect_.Contains(index_x_, index_y_))
    ++(*this);
}

TilingData::DifferenceIterator& TilingData::DifferenceIterator::operator++() {
  if (!*this)
    return *this;

  index_x_++;
  if (ignore_index_rect_.Contains(index_x_, index_y_))
    index_x_ = ignore_index_rect_.right() + 1;

  if (index_x_ > consider_index_rect_.right()) {
    index_x_ = consider_index_rect_.left();
    index_y_++;

    if (ignore_index_rect_.Contains(index_x_, index_y_)) {
      index_x_ = ignore_index_rect_.right() + 1;
      // If the ignore rect spans the whole consider rect horizontally, then
      // ignore_right + 1 will be out of bounds.
      if (ignore_index_rect_.Contains(index_x_, index_y_) ||
          index_x_ > consider_index_rect_.right()) {
        index_y_ = ignore_index_rect_.bottom() + 1;
        index_x_ = consider_index_rect_.left();
      }
    }

    if (index_y_ > consider_index_rect_.bottom())
      done();
  }

  return *this;
}

TilingData::SpiralDifferenceIterator::SpiralDifferenceIterator() {
  done();
}

TilingData::SpiralDifferenceIterator::SpiralDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect,
    const gfx::Rect& center_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect),
      direction_(RIGHT),
      delta_x_(1),
      delta_y_(0),
      current_step_(0),
      horizontal_step_count_(0),
      vertical_step_count_(0) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  // Determine around left, such that it is between -1 and num_tiles_x.
  int around_left = 0;
  if (center_rect.x() < 0 || center_rect.IsEmpty())
    around_left = -1;
  else if (center_rect.x() >= tiling_data->tiling_size().width())
    around_left = tiling_data->num_tiles_x();
  else
    around_left = tiling_data->TileXIndexFromSrcCoord(center_rect.x());

  // Determine around top, such that it is between -1 and num_tiles_y.
  int around_top = 0;
  if (center_rect.y() < 0 || center_rect.IsEmpty())
    around_top = -1;
  else if (center_rect.y() >= tiling_data->tiling_size().height())
    around_top = tiling_data->num_tiles_y();
  else
    around_top = tiling_data->TileYIndexFromSrcCoord(center_rect.y());

  // Determine around right, such that it is between -1 and num_tiles_x.
  int right_src_coord = center_rect.right() - 1;
  int around_right = 0;
  if (right_src_coord < 0 || center_rect.IsEmpty()) {
    around_right = -1;
  } else if (right_src_coord >= tiling_data->tiling_size().width()) {
    around_right = tiling_data->num_tiles_x();
  } else {
    around_right = tiling_data->TileXIndexFromSrcCoord(right_src_coord);
  }

  // Determine around bottom, such that it is between -1 and num_tiles_y.
  int bottom_src_coord = center_rect.bottom() - 1;
  int around_bottom = 0;
  if (bottom_src_coord < 0 || center_rect.IsEmpty()) {
    around_bottom = -1;
  } else if (bottom_src_coord >= tiling_data->tiling_size().height()) {
    around_bottom = tiling_data->num_tiles_y();
  } else {
    around_bottom = tiling_data->TileYIndexFromSrcCoord(bottom_src_coord);
  }

  vertical_step_count_ = around_bottom - around_top + 1;
  horizontal_step_count_ = around_right - around_left + 1;
  current_step_ = horizontal_step_count_ - 1;

  index_x_ = around_right;
  index_y_ = around_bottom;

  // The current index is the bottom right of the around rect, which is also
  // ignored. So we have to advance.
  ++(*this);
}

TilingData::SpiralDifferenceIterator& TilingData::SpiralDifferenceIterator::
operator++() {
  int cannot_hit_consider_count = 0;
  while (cannot_hit_consider_count < 4) {
    if (needs_direction_switch())
      switch_direction();

    index_x_ += delta_x_;
    index_y_ += delta_y_;
    ++current_step_;

    if (consider_index_rect_.Contains(index_x_, index_y_)) {
      cannot_hit_consider_count = 0;

      if (!ignore_index_rect_.Contains(index_x_, index_y_))
        break;

      // Steps needed to reach the very edge of the ignore rect, while remaining
      // inside (so that the continue would take us outside).
      int steps_to_edge = 0;
      switch (direction_) {
        case UP:
          steps_to_edge = index_y_ - ignore_index_rect_.top();
          break;
        case LEFT:
          steps_to_edge = index_x_ - ignore_index_rect_.left();
          break;
        case DOWN:
          steps_to_edge = ignore_index_rect_.bottom() - index_y_;
          break;
        case RIGHT:
          steps_to_edge = ignore_index_rect_.right() - index_x_;
          break;
      }

      // We need to switch directions in |max_steps|.
      int max_steps = current_step_count() - current_step_;

      int steps_to_take = std::min(steps_to_edge, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;
    } else {
      int max_steps = current_step_count() - current_step_;
      int steps_to_take = max_steps;
      bool can_hit_consider_rect = false;
      switch (direction_) {
        case UP:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.bottom() < index_y_)
            steps_to_take = index_y_ - consider_index_rect_.bottom() - 1;
          can_hit_consider_rect |= consider_index_rect_.right() >= index_x_;
          break;
        case LEFT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.right() < index_x_)
            steps_to_take = index_x_ - consider_index_rect_.right() - 1;
          can_hit_consider_rect |= consider_index_rect_.top() <= index_y_;
          break;
        case DOWN:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.top() > index_y_)
            steps_to_take = consider_index_rect_.top() - index_y_ - 1;
          can_hit_consider_rect |= consider_index_rect_.left() <= index_x_;
          break;
        case RIGHT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.left() > index_x_)
            steps_to_take = consider_index_rect_.left() - index_x_ - 1;
          can_hit_consider_rect |= consider_index_rect_.bottom() >= index_y_;
          break;
      }
      steps_to_take = std::min(steps_to_take, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;

      if (can_hit_consider_rect)
        cannot_hit_consider_count = 0;
      else
        ++cannot_hit_consider_count;
    }
  }

  if (cannot_hit_consider_count >= 4)
    done();
  return *this;
}

bool TilingData::SpiralDifferenceIterator::needs_direction_switch() const {
  return current_step_ >= current_step_count();
}

void TilingData::SpiralDifferenceIterator::switch_direction() {
  // Note that delta_x_ and delta_y_ always remain between -1 and 1.
  int new_delta_x_ = delta_y_;
  delta_y_ = -delta_x_;
  delta_x_ = new_delta_x_;

  current_step_ = 0;
  direction_ = static_cast<Direction>((direction_ + 1) % 4);

  if (direction_ == RIGHT || direction_ == LEFT) {
    ++vertical_step_count_;
    ++horizontal_step_count_;
  }
}

TilingData::ReverseSpiralDifferenceIterator::ReverseSpiralDifferenceIterator()
    : around_index_rect_(kNonPositiveQuadrantIndexRect) {
  done();
}

TilingData::ReverseSpiralDifferenceIterator::ReverseSpiralDifferenceIterator(
    const TilingData* tiling_data,
    const gfx::Rect& consider_rect,
    const gfx::Rect& ignore_rect,
    const gfx::Rect& center_rect)
    : BaseDifferenceIterator(tiling_data, consider_rect, ignore_rect),
      around_index_rect_(kNonPositiveQuadrantIndexRect),
      direction_(LEFT),
      delta_x_(-1),
      delta_y_(0),
      current_step_(0),
      horizontal_step_count_(0),
      vertical_step_count_(0) {
  if (!HasConsiderRect()) {
    done();
    return;
  }

  int around_left = 0;
  // Determine around left, such that it is between -1 and num_tiles_x.
  if (center_rect.x() < 0 || center_rect.IsEmpty())
    around_left = -1;
  else if (center_rect.x() >= tiling_data->tiling_size().width())
    around_left = tiling_data->num_tiles_x();
  else
    around_left = tiling_data->TileXIndexFromSrcCoord(center_rect.x());

  // Determine around top, such that it is between -1 and num_tiles_y.
  int around_top = 0;
  if (center_rect.y() < 0 || center_rect.IsEmpty())
    around_top = -1;
  else if (center_rect.y() >= tiling_data->tiling_size().height())
    around_top = tiling_data->num_tiles_y();
  else
    around_top = tiling_data->TileYIndexFromSrcCoord(center_rect.y());

  // Determine around right, such that it is between -1 and num_tiles_x.
  int around_right = 0;
  int right_src_coord = center_rect.right() - 1;
  if (right_src_coord < 0 || center_rect.IsEmpty()) {
    around_right = -1;
  } else if (right_src_coord >= tiling_data->tiling_size().width()) {
    around_right = tiling_data->num_tiles_x();
  } else {
    around_right = tiling_data->TileXIndexFromSrcCoord(right_src_coord);
  }

  // Determine around bottom, such that it is between -1 and num_tiles_y.
  int around_bottom = 0;
  int bottom_src_coord = center_rect.bottom() - 1;
  if (bottom_src_coord < 0 || center_rect.IsEmpty()) {
    around_bottom = -1;
  } else if (bottom_src_coord >= tiling_data->tiling_size().height()) {
    around_bottom = tiling_data->num_tiles_y();
  } else {
    around_bottom = tiling_data->TileYIndexFromSrcCoord(bottom_src_coord);
  }

  around_index_rect_ =
      IndexRect(around_left, around_right, around_top, around_bottom);
  DCHECK(around_index_rect_.is_valid());

  // Figure out the maximum distance from the around edge to consider edge.
  int max_distance = 0;
  max_distance = std::max(
      max_distance, around_index_rect_.top() - consider_index_rect_.top());
  max_distance = std::max(
      max_distance, around_index_rect_.left() - consider_index_rect_.left());
  max_distance = std::max(max_distance, consider_index_rect_.bottom() -
                                            around_index_rect_.bottom());
  max_distance = std::max(
      max_distance, consider_index_rect_.right() - around_index_rect_.right());

  // The step count is the length of the edge
  // (around_index_rect_.num_indices_x()) plus twice the max distance to pad
  // (to the right and to the left). This way the initial rect is the size
  // proportional to the center, but big enough to cover the consider rect.
  //
  // C = consider rect
  // A = around rect
  // . = area of the padded around rect
  // md = max distance (note in the picture below, there's md written vertically
  //      as well).
  // I = initial starting position
  //
  //       |md|  |md|
  //
  //     - ..........
  //     m ..........
  //     d ..........
  //     - CCCCCCC...
  //       CCCCAAC...
  //       CCCCAAC...
  //     - ..........
  //     m ..........
  //     d ..........
  //     - ..........I
  vertical_step_count_ = around_index_rect_.num_indices_y() + 2 * max_distance;
  horizontal_step_count_ =
      around_index_rect_.num_indices_x() + 2 * max_distance;

  // Start with one to the right of the padded around rect.
  index_x_ = around_index_rect_.right() + max_distance + 1;
  index_y_ = around_index_rect_.bottom() + max_distance;

  // The current index is outside a valid tile, so advance immediately.
  ++(*this);
}

TilingData::ReverseSpiralDifferenceIterator&
    TilingData::ReverseSpiralDifferenceIterator::
    operator++() {
  while (!around_index_rect_.Contains(index_x_, index_y_)) {
    if (needs_direction_switch())
      switch_direction();

    index_x_ += delta_x_;
    index_y_ += delta_y_;
    ++current_step_;

    if (around_index_rect_.Contains(index_x_, index_y_)) {
      break;
    } else if (consider_index_rect_.Contains(index_x_, index_y_)) {
      // If the tile is in the consider rect but not in ignore rect, then it's a
      // valid tile to visit.
      if (!ignore_index_rect_.Contains(index_x_, index_y_))
        break;

      // Steps needed to reach the very edge of the ignore rect, while remaining
      // inside it (so that the continue would take us outside).
      int steps_to_edge = 0;
      switch (direction_) {
        case UP:
          steps_to_edge = index_y_ - ignore_index_rect_.top();
          break;
        case LEFT:
          steps_to_edge = index_x_ - ignore_index_rect_.left();
          break;
        case DOWN:
          steps_to_edge = ignore_index_rect_.bottom() - index_y_;
          break;
        case RIGHT:
          steps_to_edge = ignore_index_rect_.right() - index_x_;
          break;
      }

      // We need to switch directions in |max_steps|.
      int max_steps = current_step_count() - current_step_;

      int steps_to_take = std::min(steps_to_edge, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;
    } else {
      // We're not in the consider rect.

      int max_steps = current_step_count() - current_step_;
      int steps_to_take = max_steps;

      // We might hit the consider rect before needing to switch directions:
      // update steps to take.
      switch (direction_) {
        case UP:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.bottom() < index_y_)
            steps_to_take = index_y_ - consider_index_rect_.bottom() - 1;
          break;
        case LEFT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.right() < index_x_)
            steps_to_take = index_x_ - consider_index_rect_.right() - 1;
          break;
        case DOWN:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.top() > index_y_)
            steps_to_take = consider_index_rect_.top() - index_y_ - 1;
          break;
        case RIGHT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.left() > index_x_)
            steps_to_take = consider_index_rect_.left() - index_x_ - 1;
          break;
      }
      steps_to_take = std::min(steps_to_take, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;
    }
  }

  // Once we enter the around rect, we're done.
  if (around_index_rect_.Contains(index_x_, index_y_))
    done();
  return *this;
}

bool TilingData::ReverseSpiralDifferenceIterator::needs_direction_switch()
    const {
  return current_step_ >= current_step_count();
}

void TilingData::ReverseSpiralDifferenceIterator::switch_direction() {
  // Note that delta_x_ and delta_y_ always remain between -1 and 1.
  int new_delta_y_ = delta_x_;
  delta_x_ = -delta_y_;
  delta_y_ = new_delta_y_;

  current_step_ = 0;
  direction_ = static_cast<Direction>((direction_ + 1) % 4);

  if (direction_ == UP || direction_ == DOWN) {
    --vertical_step_count_;
    --horizontal_step_count_;

    // We should always end up in an around rect at some point.
    // Since the direction is now vertical, we have to ensure that we will
    // advance.
    DCHECK_GE(horizontal_step_count_, 1);
    DCHECK_GE(vertical_step_count_, 1);
  }
}

}  // namespace cc

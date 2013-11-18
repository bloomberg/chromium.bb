// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_pile_base.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect_conversions.h"

namespace {
// Dimensions of the tiles in this picture pile as well as the dimensions of
// the base picture in each tile.
const int kBasePictureSize = 512;
const int kTileGridBorderPixels = 1;
#ifdef NDEBUG
const bool kDefaultClearCanvasSetting = false;
#else
const bool kDefaultClearCanvasSetting = true;
#endif
}  // namespace

namespace cc {

PicturePileBase::PicturePileBase()
    : min_contents_scale_(0),
      background_color_(SkColorSetARGBInline(0, 0, 0, 0)),
      slow_down_raster_scale_factor_for_debug_(0),
      contents_opaque_(false),
      show_debug_picture_borders_(false),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      num_raster_threads_(0) {
  tiling_.SetMaxTextureSize(gfx::Size(kBasePictureSize, kBasePictureSize));
  tile_grid_info_.fTileInterval.setEmpty();
  tile_grid_info_.fMargin.setEmpty();
  tile_grid_info_.fOffset.setZero();
}

PicturePileBase::PicturePileBase(const PicturePileBase* other)
    : picture_map_(other->picture_map_),
      tiling_(other->tiling_),
      recorded_region_(other->recorded_region_),
      min_contents_scale_(other->min_contents_scale_),
      tile_grid_info_(other->tile_grid_info_),
      background_color_(other->background_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      contents_opaque_(other->contents_opaque_),
      show_debug_picture_borders_(other->show_debug_picture_borders_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      num_raster_threads_(other->num_raster_threads_) {
}

PicturePileBase::PicturePileBase(
    const PicturePileBase* other, unsigned thread_index)
    : tiling_(other->tiling_),
      recorded_region_(other->recorded_region_),
      min_contents_scale_(other->min_contents_scale_),
      tile_grid_info_(other->tile_grid_info_),
      background_color_(other->background_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      contents_opaque_(other->contents_opaque_),
      show_debug_picture_borders_(other->show_debug_picture_borders_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      num_raster_threads_(other->num_raster_threads_) {
  for (PictureMap::const_iterator it = other->picture_map_.begin();
       it != other->picture_map_.end();
       ++it) {
    picture_map_[it->first] = it->second.CloneForThread(thread_index);
  }
}

PicturePileBase::~PicturePileBase() {
}

void PicturePileBase::Resize(gfx::Size new_size) {
  if (size() == new_size)
    return;

  gfx::Size old_size = size();
  tiling_.SetTotalSize(new_size);

  // Find all tiles that contain any pixels outside the new size.
  std::vector<PictureMapKey> to_erase;
  int min_toss_x = tiling_.FirstBorderTileXIndexFromSrcCoord(
      std::min(old_size.width(), new_size.width()));
  int min_toss_y = tiling_.FirstBorderTileYIndexFromSrcCoord(
      std::min(old_size.height(), new_size.height()));
  for (PictureMap::const_iterator it = picture_map_.begin();
       it != picture_map_.end();
       ++it) {
    const PictureMapKey& key = it->first;
    if (key.first < min_toss_x && key.second < min_toss_y)
      continue;
    to_erase.push_back(key);
  }

  for (size_t i = 0; i < to_erase.size(); ++i)
    picture_map_.erase(to_erase[i]);
}

void PicturePileBase::SetMinContentsScale(float min_contents_scale) {
  DCHECK(min_contents_scale);
  if (min_contents_scale_ == min_contents_scale)
    return;

  // Picture contents are played back scaled. When the final contents scale is
  // less than 1 (i.e. low res), then multiple recorded pixels will be used
  // to raster one final pixel.  To avoid splitting a final pixel across
  // pictures (which would result in incorrect rasterization due to blending), a
  // buffer margin is added so that any picture can be snapped to integral
  // final pixels.
  //
  // For example, if a 1/4 contents scale is used, then that would be 3 buffer
  // pixels, since that's the minimum number of pixels to add so that resulting
  // content can be snapped to a four pixel aligned grid.
  int buffer_pixels = static_cast<int>(ceil(1 / min_contents_scale) - 1);
  buffer_pixels = std::max(0, buffer_pixels);
  SetBufferPixels(buffer_pixels);
  min_contents_scale_ = min_contents_scale;
}

// static
void PicturePileBase::ComputeTileGridInfo(
    gfx::Size tile_grid_size,
    SkTileGridPicture::TileGridInfo* info) {
  DCHECK(info);
  info->fTileInterval.set(tile_grid_size.width() - 2 * kTileGridBorderPixels,
                          tile_grid_size.height() - 2 * kTileGridBorderPixels);
  DCHECK_GT(info->fTileInterval.width(), 0);
  DCHECK_GT(info->fTileInterval.height(), 0);
  info->fMargin.set(kTileGridBorderPixels, kTileGridBorderPixels);
  // Offset the tile grid coordinate space to take into account the fact
  // that the top-most and left-most tiles do not have top and left borders
  // respectively.
  info->fOffset.set(-kTileGridBorderPixels, -kTileGridBorderPixels);
}

void PicturePileBase::SetTileGridSize(gfx::Size tile_grid_size) {
  ComputeTileGridInfo(tile_grid_size, &tile_grid_info_);
}

void PicturePileBase::SetBufferPixels(int new_buffer_pixels) {
  if (new_buffer_pixels == buffer_pixels())
    return;

  Clear();
  tiling_.SetBorderTexels(new_buffer_pixels);
}

void PicturePileBase::Clear() {
  picture_map_.clear();
}

void PicturePileBase::UpdateRecordedRegion() {
  recorded_region_.Clear();
  for (PictureMap::const_iterator it = picture_map_.begin();
       it != picture_map_.end();
       ++it) {
    if (it->second.picture.get()) {
      const PictureMapKey& key = it->first;
      recorded_region_.Union(tile_bounds(key.first, key.second));
    }
  }
}

bool PicturePileBase::HasRecordingAt(int x, int y) {
  PictureMap::const_iterator found = picture_map_.find(PictureMapKey(x, y));
  if (found == picture_map_.end())
    return false;
  return !!found->second.picture.get();
}

bool PicturePileBase::CanRaster(float contents_scale, gfx::Rect content_rect) {
  if (tiling_.total_size().IsEmpty())
    return false;
  gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
      content_rect, 1.f / contents_scale);
  layer_rect.Intersect(gfx::Rect(tiling_.total_size()));
  return recorded_region_.Contains(layer_rect);
}

gfx::Rect PicturePileBase::PaddedRect(const PictureMapKey& key) {
  gfx::Rect tile = tiling_.TileBounds(key.first, key.second);
  tile.Inset(
      -buffer_pixels(), -buffer_pixels(), -buffer_pixels(), -buffer_pixels());
  return tile;
}

scoped_ptr<base::Value> PicturePileBase::AsValue() const {
  scoped_ptr<base::ListValue> pictures(new base::ListValue());
  gfx::Rect layer_rect(tiling_.total_size());
  std::set<void*> appended_pictures;
  for (TilingData::Iterator tile_iter(&tiling_, layer_rect);
       tile_iter; ++tile_iter) {
    PictureMap::const_iterator map_iter = picture_map_.find(tile_iter.index());
    if (map_iter == picture_map_.end())
      continue;

    Picture* picture = map_iter->second.picture.get();
    if (picture && (appended_pictures.count(picture) == 0)) {
      appended_pictures.insert(picture);
      pictures->Append(TracedValue::CreateIDRef(picture).release());
    }
  }
  return pictures.PassAs<base::Value>();
}

PicturePileBase::PictureInfo::PictureInfo() {}

PicturePileBase::PictureInfo::~PictureInfo() {}

bool PicturePileBase::PictureInfo::Invalidate() {
  if (!picture.get())
    return false;
  picture = NULL;
  return true;
}

PicturePileBase::PictureInfo PicturePileBase::PictureInfo::CloneForThread(
    int thread_index) const {
  PictureInfo info = *this;
  if (picture.get())
    info.picture = picture->GetCloneForDrawingOnThread(thread_index);
  return info;
}

}  // namespace cc

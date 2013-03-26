// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_pile_base.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect_conversions.h"

namespace {
// Dimensions of the tiles in this picture pile as well as the dimensions of
// the base picture in each tile.
const int kBasePictureSize = 3000;
const int kTileGridBorderPixels = 1;
}

namespace cc {

PicturePileBase::PicturePileBase()
    : min_contents_scale_(0),
      background_color_(SkColorSetARGBInline(0, 0, 0, 0)),
      slow_down_raster_scale_factor_for_debug_(0),
      num_raster_threads_(0) {
  tiling_.SetMaxTextureSize(gfx::Size(kBasePictureSize, kBasePictureSize));
  tile_grid_info_.fTileInterval.setEmpty();
  tile_grid_info_.fMargin.setEmpty();
  tile_grid_info_.fOffset.setZero();
}

PicturePileBase::PicturePileBase(const PicturePileBase* other)
    : picture_list_map_(other->picture_list_map_),
      tiling_(other->tiling_),
      recorded_region_(other->recorded_region_),
      min_contents_scale_(other->min_contents_scale_),
      tile_grid_info_(other->tile_grid_info_),
      background_color_(other->background_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
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
      num_raster_threads_(other->num_raster_threads_) {
  const PictureListMap& other_pic_list_map = other->picture_list_map_;
  for (PictureListMap::const_iterator map_iter = other_pic_list_map.begin();
       map_iter != other_pic_list_map.end(); ++map_iter) {
    PictureList& pic_list = picture_list_map_[map_iter->first];
    const PictureList& other_pic_list = map_iter->second;
    for (PictureList::const_iterator pic_iter = other_pic_list.begin();
         pic_iter != other_pic_list.end(); ++pic_iter) {
      pic_list.push_back(
          (*pic_iter)->GetCloneForDrawingOnThread(thread_index));
    }
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
  std::vector<PictureListMapKey> to_erase;
  int min_toss_x = tiling_.FirstBorderTileXIndexFromSrcCoord(
      std::min(old_size.width(), new_size.width()));
  int min_toss_y = tiling_.FirstBorderTileYIndexFromSrcCoord(
      std::min(old_size.height(), new_size.height()));
  for (PictureListMap::iterator iter = picture_list_map_.begin();
       iter != picture_list_map_.end(); ++iter) {
    if (iter->first.first < min_toss_x && iter->first.second < min_toss_y)
      continue;
    to_erase.push_back(iter->first);
  }

  for (size_t i = 0; i < to_erase.size(); ++i)
    picture_list_map_.erase(to_erase[i]);
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

void PicturePileBase::SetTileGridSize(const gfx::Size& tile_grid_size) {
  tile_grid_info_.fTileInterval.set(
      tile_grid_size.width() - 2 * kTileGridBorderPixels,
      tile_grid_size.height() - 2 * kTileGridBorderPixels);
  DCHECK_GT(tile_grid_info_.fTileInterval.width(), 0);
  DCHECK_GT(tile_grid_info_.fTileInterval.height(), 0);
  tile_grid_info_.fMargin.set(kTileGridBorderPixels,
      kTileGridBorderPixels);
  // Offset the tile grid coordinate space to take into account the fact
  // that the top-most and left-most tiles do not have top and left borders
  // respectively.
  tile_grid_info_.fOffset.set(-kTileGridBorderPixels,
      -kTileGridBorderPixels);
}

void PicturePileBase::SetBufferPixels(int new_buffer_pixels) {
  if (new_buffer_pixels == buffer_pixels())
    return;

  Clear();
  tiling_.SetBorderTexels(new_buffer_pixels);
}

void PicturePileBase::Clear() {
  picture_list_map_.clear();
}

void PicturePileBase::UpdateRecordedRegion() {
  recorded_region_.Clear();
  for (int x = 0; x < num_tiles_x(); ++x) {
    for (int y = 0; y < num_tiles_y(); ++y) {
      if (!HasRecordingAt(x, y))
        continue;
      recorded_region_.Union(tile_bounds(x, y));
    }
  }
}

bool PicturePileBase::HasRecordingAt(int x, int y) {
  PictureListMap::iterator found =
      picture_list_map_.find(PictureListMapKey(x, y));
  if (found == picture_list_map_.end())
    return false;
  DCHECK(!found->second.empty());
  return true;
}

bool PicturePileBase::CanRaster(float contents_scale, gfx::Rect content_rect) {
  if (tiling_.total_size().IsEmpty())
    return false;
  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));
  layer_rect.Intersect(gfx::Rect(tiling_.total_size()));
  return recorded_region_.Contains(layer_rect);
}

}  // namespace cc

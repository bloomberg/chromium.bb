// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_pile_base.h"

#include "base/logging.h"

namespace {
// Dimensions of the tiles in this picture pile as well as the dimensions of
// the base picture in each tile.
const int kBasePictureSize = 3000;
}

namespace cc {

PicturePileBase::PicturePileBase()
    : min_contents_scale_(0) {
  tiling_.SetMaxTextureSize(gfx::Size(kBasePictureSize, kBasePictureSize));
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
  int min_toss_x = tiling_.BorderTileXIndexFromSrcCoord(
      std::min(old_size.width(), new_size.width()));
  int min_toss_y = tiling_.BorderTileYIndexFromSrcCoord(
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

void PicturePileBase::SetBufferPixels(int new_buffer_pixels) {
  if (new_buffer_pixels == buffer_pixels())
    return;

  Clear();
  tiling_.SetBorderTexels(new_buffer_pixels);
}

void PicturePileBase::Clear() {
  picture_list_map_.clear();
}

void PicturePileBase::PushPropertiesTo(PicturePileBase* other) {
  other->picture_list_map_ = picture_list_map_;
  other->tiling_ = tiling_;
  other->recorded_region_ = recorded_region_;
  other->min_contents_scale_ = min_contents_scale_;
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

}  // namespace cc

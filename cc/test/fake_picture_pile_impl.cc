// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_pile_impl.h"

#include <limits>
#include <utility>

#include "cc/test/impl_side_painting_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakePicturePileImpl::FakePicturePileImpl() {}

FakePicturePileImpl::~FakePicturePileImpl() {}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreateFilledPile(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds) {
  scoped_refptr<FakePicturePileImpl> pile(new FakePicturePileImpl());
  pile->tiling().SetTotalSize(layer_bounds);
  pile->tiling().SetMaxTextureSize(tile_size);
  pile->SetTileGridSize(ImplSidePaintingSettings().default_tile_size);
  for (int x = 0; x < pile->tiling().num_tiles_x(); ++x) {
    for (int y = 0; y < pile->tiling().num_tiles_y(); ++y)
      pile->AddRecordingAt(x, y);
  }
  pile->UpdateRecordedRegion();
  return pile;
}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreateEmptyPile(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds) {
  scoped_refptr<FakePicturePileImpl> pile(new FakePicturePileImpl());
  pile->tiling().SetTotalSize(layer_bounds);
  pile->tiling().SetMaxTextureSize(tile_size);
  pile->SetTileGridSize(ImplSidePaintingSettings().default_tile_size);
  pile->UpdateRecordedRegion();
  return pile;
}

scoped_refptr<FakePicturePileImpl>
FakePicturePileImpl::CreatePileWithRecordedRegion(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds,
    const Region& recorded_region) {
  scoped_refptr<FakePicturePileImpl> pile(new FakePicturePileImpl());
  pile->tiling().SetTotalSize(layer_bounds);
  pile->tiling().SetMaxTextureSize(tile_size);
  pile->SetTileGridSize(ImplSidePaintingSettings().default_tile_size);
  pile->SetRecordedRegionForTesting(recorded_region);
  return pile;
}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreatePile() {
  scoped_refptr<FakePicturePileImpl> pile(new FakePicturePileImpl());
  gfx::Size size(std::numeric_limits<int>::max(),
                 std::numeric_limits<int>::max());
  pile->Resize(size);
  pile->recorded_region_ = Region(gfx::Rect(size));
  return pile;
}

void FakePicturePileImpl::AddRecordingAt(int x, int y) {
  EXPECT_GE(x, 0);
  EXPECT_GE(y, 0);
  EXPECT_LT(x, tiling_.num_tiles_x());
  EXPECT_LT(y, tiling_.num_tiles_y());

  if (HasRecordingAt(x, y))
    return;
  gfx::Rect bounds(tiling().TileBounds(x, y));
  bounds.Inset(-buffer_pixels(), -buffer_pixels());

  scoped_refptr<Picture> picture(
      Picture::Create(bounds, &client_, tile_grid_info_, true, 0));
  picture_map_[std::pair<int, int>(x, y)].SetPicture(picture);
  EXPECT_TRUE(HasRecordingAt(x, y));

  UpdateRecordedRegion();
}

void FakePicturePileImpl::RemoveRecordingAt(int x, int y) {
  EXPECT_GE(x, 0);
  EXPECT_GE(y, 0);
  EXPECT_LT(x, tiling_.num_tiles_x());
  EXPECT_LT(y, tiling_.num_tiles_y());

  if (!HasRecordingAt(x, y))
    return;
  picture_map_.erase(std::pair<int, int>(x, y));
  EXPECT_FALSE(HasRecordingAt(x, y));

  UpdateRecordedRegion();
}

void FakePicturePileImpl::RerecordPile() {
  for (int y = 0; y < num_tiles_y(); ++y) {
    for (int x = 0; x < num_tiles_x(); ++x) {
      RemoveRecordingAt(x, y);
      AddRecordingAt(x, y);
    }
  }
}

}  // namespace cc

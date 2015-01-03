// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_pile_impl.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "cc/resources/picture_pile.h"
#include "cc/test/fake_picture_pile.h"
#include "cc/test/impl_side_painting_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakePicturePileImpl::FakePicturePileImpl() : playback_allowed_event_(nullptr) {
}

FakePicturePileImpl::FakePicturePileImpl(
    const PicturePile* other,
    base::WaitableEvent* playback_allowed_event)
    : PicturePileImpl(other),
      playback_allowed_event_(playback_allowed_event),
      tile_grid_info_(other->GetTileGridInfoForTesting()) {
}

FakePicturePileImpl::~FakePicturePileImpl() {}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreateFilledPile(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds) {
  FakePicturePile pile;
  pile.tiling().SetTilingSize(layer_bounds);
  pile.tiling().SetMaxTextureSize(tile_size);
  pile.SetTileGridSize(ImplSidePaintingSettings().default_tile_grid_size);
  pile.SetRecordedViewport(gfx::Rect(layer_bounds));
  pile.SetHasAnyRecordings(true);

  scoped_refptr<FakePicturePileImpl> pile_impl(
      new FakePicturePileImpl(&pile, nullptr));
  for (int x = 0; x < pile_impl->tiling().num_tiles_x(); ++x) {
    for (int y = 0; y < pile_impl->tiling().num_tiles_y(); ++y)
      pile_impl->AddRecordingAt(x, y);
  }
  return pile_impl;
}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreateEmptyPile(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds) {
  FakePicturePile pile;
  pile.tiling().SetTilingSize(layer_bounds);
  pile.tiling().SetMaxTextureSize(tile_size);
  pile.SetTileGridSize(ImplSidePaintingSettings().default_tile_grid_size);
  pile.SetRecordedViewport(gfx::Rect());
  pile.SetHasAnyRecordings(false);
  return make_scoped_refptr(new FakePicturePileImpl(&pile, nullptr));
}

scoped_refptr<FakePicturePileImpl>
FakePicturePileImpl::CreateEmptyPileThatThinksItHasRecordings(
    const gfx::Size& tile_size,
    const gfx::Size& layer_bounds) {
  FakePicturePile pile;
  pile.tiling().SetTilingSize(layer_bounds);
  pile.tiling().SetMaxTextureSize(tile_size);
  pile.SetTileGridSize(ImplSidePaintingSettings().default_tile_grid_size);
  // This simulates a false positive for this flag.
  pile.SetRecordedViewport(gfx::Rect());
  pile.SetHasAnyRecordings(true);
  return make_scoped_refptr(new FakePicturePileImpl(&pile, nullptr));
}

scoped_refptr<FakePicturePileImpl>
FakePicturePileImpl::CreateInfiniteFilledPile() {
  FakePicturePile pile;
  gfx::Size size(std::numeric_limits<int>::max(),
                 std::numeric_limits<int>::max());
  pile.tiling().SetTilingSize(size);
  pile.tiling().SetMaxTextureSize(size);
  pile.SetTileGridSize(size);
  pile.SetRecordedViewport(gfx::Rect(size));
  pile.SetHasAnyRecordings(true);

  scoped_refptr<FakePicturePileImpl> pile_impl(
      new FakePicturePileImpl(&pile, nullptr));
  pile_impl->AddRecordingAt(0, 0);
  return pile_impl;
}

scoped_refptr<FakePicturePileImpl> FakePicturePileImpl::CreateFromPile(
    const PicturePile* other,
    base::WaitableEvent* playback_allowed_event) {
  return make_scoped_refptr(
      new FakePicturePileImpl(other, playback_allowed_event));
}

void FakePicturePileImpl::PlaybackToCanvas(SkCanvas* canvas,
                                           const gfx::Rect& canvas_rect,
                                           float contents_scale) const {
  if (playback_allowed_event_)
    playback_allowed_event_->Wait();
  PicturePileImpl::PlaybackToCanvas(canvas, canvas_rect, contents_scale);
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

  scoped_refptr<Picture> picture(Picture::Create(
      bounds, &client_, tile_grid_info_, true, Picture::RECORD_NORMALLY));
  picture_map_[std::pair<int, int>(x, y)].SetPicture(picture);
  EXPECT_TRUE(HasRecordingAt(x, y));

  has_any_recordings_ = true;
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
}

bool FakePicturePileImpl::HasRecordingAt(int x, int y) const {
  PictureMap::const_iterator found = picture_map_.find(PictureMapKey(x, y));
  if (found == picture_map_.end())
    return false;
  return !!found->second.GetPicture();
}

void FakePicturePileImpl::RerecordPile() {
  for (int y = 0; y < num_tiles_y(); ++y) {
    for (int x = 0; x < num_tiles_x(); ++x) {
      RemoveRecordingAt(x, y);
      AddRecordingAt(x, y);
    }
  }
}

void FakePicturePileImpl::SetMinContentsScale(float min_contents_scale) {
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

void FakePicturePileImpl::SetBufferPixels(int new_buffer_pixels) {
  if (new_buffer_pixels == buffer_pixels())
    return;

  Clear();
  tiling_.SetBorderTexels(new_buffer_pixels);
}

void FakePicturePileImpl::Clear() {
  picture_map_.clear();
  recorded_viewport_ = gfx::Rect();
}

}  // namespace cc

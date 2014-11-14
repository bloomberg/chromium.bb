// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_PILE_H_
#define CC_TEST_FAKE_PICTURE_PILE_H_

#include "cc/resources/picture_pile.h"

namespace base {
class WaitableEvent;
}

namespace cc {

class FakePicturePile : public PicturePile {
 public:
  FakePicturePile() : playback_allowed_event_(nullptr) {}
  ~FakePicturePile() override {}

  // PicturePile overrides.
  scoped_refptr<RasterSource> CreateRasterSource() const override;

  using PicturePile::buffer_pixels;
  using PicturePile::CanRasterSlowTileCheck;
  using PicturePile::Clear;

  PictureMap& picture_map() { return picture_map_; }
  const gfx::Rect& recorded_viewport() const { return recorded_viewport_; }

  bool CanRasterLayerRect(gfx::Rect layer_rect) {
    layer_rect.Intersect(gfx::Rect(tiling_.tiling_size()));
    if (recorded_viewport_.Contains(layer_rect))
      return true;
    return CanRasterSlowTileCheck(layer_rect);
  }

  bool HasRecordings() const { return has_any_recordings_; }

  void SetRecordedViewport(const gfx::Rect& viewport) {
    recorded_viewport_ = viewport;
  }

  void SetHasAnyRecordings(bool has_recordings) {
    has_any_recordings_ = has_recordings;
  }

  void SetPlaybackAllowedEvent(base::WaitableEvent* event) {
    playback_allowed_event_ = event;
  }

  TilingData& tiling() { return tiling_; }

  bool is_solid_color() const { return is_solid_color_; }
  SkColor solid_color() const { return solid_color_; }

  void SetPixelRecordDistance(int d) { pixel_record_distance_ = d; }

  typedef PicturePile::PictureInfo PictureInfo;
  typedef PicturePile::PictureMapKey PictureMapKey;
  typedef PicturePile::PictureMap PictureMap;

 private:
  base::WaitableEvent* playback_allowed_event_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_PILE_H_

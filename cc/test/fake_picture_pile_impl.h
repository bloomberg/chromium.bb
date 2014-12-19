// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_PILE_IMPL_H_
#define CC_TEST_FAKE_PICTURE_PILE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/test/fake_content_layer_client.h"

namespace base {
class WaitableEvent;
}

namespace cc {

class FakePicturePileImpl : public PicturePileImpl {
 public:
  static scoped_refptr<FakePicturePileImpl> CreateFilledPileWithDefaultTileSize(
      const gfx::Size& layer_bounds) {
    return CreateFilledPile(gfx::Size(512, 512), layer_bounds);
  }
  static scoped_refptr<FakePicturePileImpl> CreateEmptyPileWithDefaultTileSize(
      const gfx::Size& layer_bounds) {
    return CreateEmptyPile(gfx::Size(512, 512), layer_bounds);
  }
  static scoped_refptr<FakePicturePileImpl> CreateFilledPile(
      const gfx::Size& tile_size,
      const gfx::Size& layer_bounds);
  static scoped_refptr<FakePicturePileImpl> CreateEmptyPile(
      const gfx::Size& tile_size,
      const gfx::Size& layer_bounds);
  static scoped_refptr<FakePicturePileImpl>
      CreateEmptyPileThatThinksItHasRecordings(const gfx::Size& tile_size,
                                               const gfx::Size& layer_bounds);
  static scoped_refptr<FakePicturePileImpl> CreateInfiniteFilledPile();
  static scoped_refptr<FakePicturePileImpl> CreateFromPile(
      const PicturePile* other,
      base::WaitableEvent* playback_allowed_event);

  // Hi-jack the PlaybackToCanvas method to delay its completion.
  void PlaybackToCanvas(SkCanvas* canvas,
                        const gfx::Rect& canvas_rect,
                        float contents_scale) const override;

  TilingData& tiling() { return tiling_; }

  void AddRecordingAt(int x, int y);
  void RemoveRecordingAt(int x, int y);
  void RerecordPile();

  void add_draw_rect(const gfx::RectF& rect) {
    client_.add_draw_rect(rect, default_paint_);
  }

  void add_draw_bitmap(const SkBitmap& bitmap, const gfx::Point& point) {
    client_.add_draw_bitmap(bitmap, point, default_paint_);
  }

  void add_draw_rect_with_paint(const gfx::RectF& rect, const SkPaint& paint) {
    client_.add_draw_rect(rect, paint);
  }

  void add_draw_bitmap_with_paint(const SkBitmap& bitmap,
                                  const gfx::Point& point,
                                  const SkPaint& paint) {
    client_.add_draw_bitmap(bitmap, point, paint);
  }

  void set_default_paint(const SkPaint& paint) {
    default_paint_ = paint;
  }

  void set_background_color(SkColor color) {
    background_color_ = color;
  }

  void set_clear_canvas_with_debug_color(bool clear) {
    clear_canvas_with_debug_color_ = clear;
  }

  void set_is_solid_color(bool is_solid_color) {
    is_solid_color_ = is_solid_color;
  }

  bool HasRecordingAt(int x, int y) const;

  int num_tiles_x() const { return tiling_.num_tiles_x(); }
  int num_tiles_y() const { return tiling_.num_tiles_y(); }

  void SetMinContentsScale(float scale);
  void SetBufferPixels(int new_buffer_pixels);
  void Clear();

 protected:
  FakePicturePileImpl();
  FakePicturePileImpl(const PicturePile* other,
                      base::WaitableEvent* playback_allowed_event);
  ~FakePicturePileImpl() override;

  FakeContentLayerClient client_;
  SkPaint default_paint_;
  base::WaitableEvent* playback_allowed_event_;
  SkTileGridFactory::TileGridInfo tile_grid_info_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_PILE_IMPL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_PILE_IMPL_H_
#define CC_TEST_FAKE_PICTURE_PILE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/test/fake_content_layer_client.h"

namespace cc {

class FakePicturePileImpl : public PicturePileImpl {
 public:
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

  void set_contents_opaque(bool contents_opaque) {
    contents_opaque_ = contents_opaque;
  }

  void set_contents_fill_bounds_completely(bool fills) {
    contents_fill_bounds_completely_ = fills;
  }

  void set_clear_canvas_with_debug_color(bool clear) {
    clear_canvas_with_debug_color_ = clear;
  }

 protected:
  FakePicturePileImpl();
  virtual ~FakePicturePileImpl();

  FakeContentLayerClient client_;
  SkPaint default_paint_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_PILE_IMPL_H_

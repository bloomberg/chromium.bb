// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "cc/layers/content_layer_client.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class FakeContentLayerClient : public ContentLayerClient {
 public:
  struct BitmapData {
    SkBitmap bitmap;
    gfx::Point point;
    SkPaint paint;
  };

  FakeContentLayerClient();
  ~FakeContentLayerClient() override;

  void PaintContents(SkCanvas* canvas,
                     const gfx::Rect& rect,
                     PaintingControlSetting painting_control) override;
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      const gfx::Rect& clip,
      PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;

  void set_fill_with_nonsolid_color(bool nonsolid) {
    fill_with_nonsolid_color_ = nonsolid;
  }

  void add_draw_rect(const gfx::RectF& rect, const SkPaint& paint) {
    draw_rects_.push_back(std::make_pair(rect, paint));
  }

  void add_draw_bitmap(const SkBitmap& bitmap,
                       const gfx::Point& point,
                       const SkPaint& paint) {
    BitmapData data;
    data.bitmap = bitmap;
    data.point = point;
    data.paint = paint;
    draw_bitmaps_.push_back(data);
  }

  SkCanvas* last_canvas() const { return last_canvas_; }

  PaintingControlSetting last_painting_control() const {
    return last_painting_control_;
  }

 private:
  typedef std::vector<std::pair<gfx::RectF, SkPaint>> RectPaintVector;
  typedef std::vector<BitmapData> BitmapVector;

  bool fill_with_nonsolid_color_;
  RectPaintVector draw_rects_;
  BitmapVector draw_bitmaps_;
  SkCanvas* last_canvas_;
  PaintingControlSetting last_painting_control_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

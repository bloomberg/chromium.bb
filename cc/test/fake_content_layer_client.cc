// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FakeContentLayerClient::FakeContentLayerClient()
    : fill_with_nonsolid_color_(false), last_canvas_(NULL) {
}

FakeContentLayerClient::~FakeContentLayerClient() {
}

void FakeContentLayerClient::PaintContents(
    SkCanvas* canvas,
    const gfx::Rect& paint_rect,
    ContentLayerClient::GraphicsContextStatus gc_status) {
  last_canvas_ = canvas;
  last_context_status_ = gc_status;

  canvas->clipRect(gfx::RectToSkRect(paint_rect));
  for (RectPaintVector::const_iterator it = draw_rects_.begin();
      it != draw_rects_.end(); ++it) {
    const gfx::RectF& draw_rect = it->first;
    const SkPaint& paint = it->second;
    canvas->drawRectCoords(draw_rect.x(),
                           draw_rect.y(),
                           draw_rect.right(),
                           draw_rect.bottom(),
                           paint);
  }

  for (BitmapVector::const_iterator it = draw_bitmaps_.begin();
      it != draw_bitmaps_.end(); ++it) {
    canvas->drawBitmap(it->bitmap, it->point.x(), it->point.y(), &it->paint);
  }

  if (fill_with_nonsolid_color_) {
    gfx::RectF draw_rect = paint_rect;
    bool red = true;
    while (!draw_rect.IsEmpty()) {
      SkPaint paint;
      paint.setColor(red ? SK_ColorRED : SK_ColorBLUE);
      canvas->drawRect(gfx::RectFToSkRect(draw_rect), paint);
      draw_rect.Inset(1, 1);
    }
  }
}

bool FakeContentLayerClient::FillsBoundsCompletely() const { return false; }

}  // namespace cc

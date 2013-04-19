// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

FakeContentLayerClient::FakeContentLayerClient()
    : paint_all_opaque_(false) {
}

FakeContentLayerClient::~FakeContentLayerClient() {
}

void FakeContentLayerClient::PaintContents(SkCanvas* canvas,
    gfx::Rect rect, gfx::RectF* opaque_rect) {
  if (paint_all_opaque_)
    *opaque_rect = rect;

  for (RectPaintVector::const_iterator it = draw_rects_.begin();
      it < draw_rects_.end(); ++it) {
    gfx::Rect rect = it->first;
    SkPaint paint = it->second;
    SkRect draw_rect = SkRect::MakeXYWH(
        rect.x(),
        rect.y(),
        rect.width(),
        rect.height());
    canvas->drawRect(draw_rect, paint);
  }
}

}  // namespace cc

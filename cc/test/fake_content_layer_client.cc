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

void FakeContentLayerClient::paintContents(SkCanvas* canvas,
    const gfx::Rect& rect, gfx::RectF& opaque_rect) {
  if (paint_all_opaque_)
    opaque_rect = rect;

  SkPaint paint;
  for (RectVector::const_iterator rect_it = draw_rects_.begin();
      rect_it < draw_rects_.end(); rect_it++) {
    SkRect draw_rect = SkRect::MakeXYWH(rect_it->x(), rect_it->y(),
        rect_it->width(), rect_it->height());
    canvas->drawRect(draw_rect, paint);
  }
}

}  // namespace cc

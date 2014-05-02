// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/solid_color_content_layer_client.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

namespace cc {

void SolidColorContentLayerClient::PaintContents(
    SkCanvas* canvas,
    const gfx::Rect& rect,
    gfx::RectF* opaque_rect,
    ContentLayerClient::GraphicsContextStatus gc_status) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(color_);

  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawRect(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
      paint);

  if (SkColorGetA(color_) == 255)
    *opaque_rect = rect;
}

bool SolidColorContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

}  // namespace cc

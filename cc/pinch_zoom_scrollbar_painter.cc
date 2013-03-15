// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/pinch_zoom_scrollbar_painter.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect.h"

namespace cc {

PinchZoomScrollbarPainter::~PinchZoomScrollbarPainter() {}

void PinchZoomScrollbarPainter::PaintScrollbarBackground(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintTrackBackground(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintBackTrackPart(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintForwardTrackPart(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintBackButtonStart(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintBackButtonEnd(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintForwardButtonStart(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintForwardButtonEnd(
    SkCanvas*,
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintTickmarks(
    SkCanvas*, 
    const gfx::Rect&) {
}

void PinchZoomScrollbarPainter::PaintThumb(
    SkCanvas* canvas,
    const gfx::Rect& thumb_rect) {
  canvas->clear(SkColorSetARGB(0, 0, 0, 0));
  SkPaint paint;

  // TODO(wjmaclean): currently the pinch-zoom overlay scrollbars are
  // drawn as grey, but need to check this with UX design.
  paint.setColor(SkColorSetARGB(128, 32, 32, 32));
  SkScalar border = 2;
  SkScalar corner_radius = 2;
  SkRect rect = SkRect::MakeXYWH(border, border,
                                 thumb_rect.width() - 2 * border,
                                 thumb_rect.height() - 2 * border);
  canvas->drawRoundRect(rect, corner_radius, corner_radius, paint);
}

}  // namespace cc

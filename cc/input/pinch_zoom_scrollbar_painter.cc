// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/pinch_zoom_scrollbar_painter.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect.h"

namespace cc {

PinchZoomScrollbarPainter::~PinchZoomScrollbarPainter() {}

void PinchZoomScrollbarPainter::PaintScrollbarBackground(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintTrackBackground(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintBackTrackPart(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintForwardTrackPart(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintBackButtonStart(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintBackButtonEnd(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintForwardButtonStart(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintForwardButtonEnd(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintTickmarks(
    SkCanvas* canvas,
    gfx::Rect rect) {
}

void PinchZoomScrollbarPainter::PaintThumb(
    SkCanvas* canvas,
    gfx::Rect thumb_rect) {
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

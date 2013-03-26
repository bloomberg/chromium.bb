// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar_theme_painter.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect.h"

namespace cc {

void FakeScrollbarThemePainter::PaintScrollbarBackground(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintTrackBackground(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintBackTrackPart(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintForwardTrackPart(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintBackButtonStart(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintBackButtonEnd(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintForwardButtonStart(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintForwardButtonEnd(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintTickmarks(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::PaintThumb(
    SkCanvas* canvas, gfx::Rect rect) {
  Paint(canvas, rect);
}

void FakeScrollbarThemePainter::Paint(SkCanvas* canvas, gfx::Rect rect) {
  if (!paint_)
    return;
  // Fill the scrollbar with a different color each time.
  ++fill_color_;
  canvas->clear(SK_ColorBLACK | fill_color_);
}

}  // namespace cc

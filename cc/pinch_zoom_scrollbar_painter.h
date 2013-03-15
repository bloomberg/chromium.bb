// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PINCH_ZOOM_SCROLLBAR_PAINTER_H_
#define CC_PINCH_ZOOM_SCROLLBAR_PAINTER_H_

#include "base/compiler_specific.h"
#include "cc/scrollbar_theme_painter.h"

namespace cc {

class PinchZoomScrollbarPainter : public ScrollbarThemePainter {
 public:
  PinchZoomScrollbarPainter() {}
  virtual ~PinchZoomScrollbarPainter();

  virtual void PaintScrollbarBackground(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintTrackBackground(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintBackTrackPart(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintForwardTrackPart(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintBackButtonStart(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintBackButtonEnd(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintForwardButtonStart(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintForwardButtonEnd(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintTickmarks(
    SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void PaintThumb(SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
};

}  // namespace cc

#endif  // CC_PINCH_ZOOM_SCROLLBAR_PAINTER_H_

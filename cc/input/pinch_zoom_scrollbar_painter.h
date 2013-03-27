// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_PINCH_ZOOM_SCROLLBAR_PAINTER_H_
#define CC_INPUT_PINCH_ZOOM_SCROLLBAR_PAINTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cc/layers/scrollbar_theme_painter.h"

namespace cc {

class PinchZoomScrollbarPainter : public ScrollbarThemePainter {
 public:
  PinchZoomScrollbarPainter() {}
  virtual ~PinchZoomScrollbarPainter();

  virtual void PaintScrollbarBackground(SkCanvas* canvas,
                                        gfx::Rect rect) OVERRIDE;
  virtual void PaintTrackBackground(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintBackTrackPart(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintForwardTrackPart(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintBackButtonStart(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintBackButtonEnd(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintForwardButtonStart(SkCanvas* canvas,
                                       gfx::Rect rect) OVERRIDE;
  virtual void PaintForwardButtonEnd(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintTickmarks(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;
  virtual void PaintThumb(SkCanvas* canvas, gfx::Rect rect) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PinchZoomScrollbarPainter);
};

}  // namespace cc

#endif  // CC_INPUT_PINCH_ZOOM_SCROLLBAR_PAINTER_H_

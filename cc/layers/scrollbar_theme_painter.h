// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_THEME_PAINTER_H_
#define CC_LAYERS_SCROLLBAR_THEME_PAINTER_H_

#include "cc/base/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
}

namespace cc {

class CC_EXPORT ScrollbarThemePainter {
 public:
  virtual ~ScrollbarThemePainter() {}

  virtual void PaintScrollbarBackground(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintTrackBackground(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintBackTrackPart(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintForwardTrackPart(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintBackButtonStart(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintBackButtonEnd(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintForwardButtonStart(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintForwardButtonEnd(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintTickmarks(SkCanvas* canvas, gfx::Rect rect) = 0;
  virtual void PaintThumb(SkCanvas* canvas, gfx::Rect rect) = 0;
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_THEME_PAINTER_H_

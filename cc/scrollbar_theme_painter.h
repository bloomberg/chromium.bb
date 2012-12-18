// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_THEME_PAINTER_H_
#define CC_SCROLLBAR_THEME_PAINTER_H_

#include "cc/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
}

namespace cc {

class CC_EXPORT ScrollbarThemePainter {
 public:
  virtual ~ScrollbarThemePainter() {}

  virtual void PaintScrollbarBackground(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintTrackBackground(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintBackTrackPart(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintForwardTrackPart(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintBackButtonStart(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintBackButtonEnd(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintForwardButtonStart(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintForwardButtonEnd(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintTickmarks(SkCanvas*, const gfx::Rect&) = 0;
  virtual void PaintThumb(SkCanvas*, const gfx::Rect&) = 0;
};

}  // namespace cc

#endif  // CC_SCROLLBAR_THEME_PAINTER_H_

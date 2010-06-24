// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_CANVAS_H_
#define GFX_CANVAS_H_

#include "gfx/canvas_skia.h"

namespace gfx {

// Temporary compatibility shim, remove once Canvas2->Canvas.
class Canvas : public CanvasSkia {
 public:
  Canvas(int width, int height, bool is_opaque)
      : CanvasSkia(width, height, is_opaque) {
  }
  Canvas() : CanvasSkia() {}

  // Overridden from Canvas2:
  Canvas* AsCanvas() {
    return this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Canvas);
};

}

#endif  // GFX_CANVAS_H_

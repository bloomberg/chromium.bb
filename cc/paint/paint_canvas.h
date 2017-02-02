// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CANVAS_H_
#define CC_PAINT_PAINT_CANVAS_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

using PaintCanvas = SkCanvas;
using PaintCanvasNoDraw = SkNoDrawCanvas;
using PaintCanvasAutoRestore = SkAutoCanvasRestore;

class CC_PAINT_EXPORT PaintCanvasPassThrough : public SkNWayCanvas {
 public:
  explicit PaintCanvasPassThrough(SkCanvas* canvas);
  PaintCanvasPassThrough(int width, int height);
  ~PaintCanvasPassThrough() override;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_CANVAS_H_

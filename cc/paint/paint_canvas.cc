// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_canvas.h"

namespace cc {

PaintCanvasPassThrough::PaintCanvasPassThrough(SkCanvas* canvas)
    : SkNWayCanvas(canvas->getBaseLayerSize().width(),
                   canvas->getBaseLayerSize().height()) {
  SkIRect raster_bounds;
  canvas->getDeviceClipBounds(&raster_bounds);
  clipRect(SkRect::MakeFromIRect(raster_bounds));
  setMatrix(canvas->getTotalMatrix());
  addCanvas(canvas);
}

PaintCanvasPassThrough::PaintCanvasPassThrough(int width, int height)
    : SkNWayCanvas(width, height) {}

PaintCanvasPassThrough::~PaintCanvasPassThrough() = default;

}  // namespace cc

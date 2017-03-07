// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_canvas.h"

#include "third_party/skia/include/core/SkMetaData.h"

#if defined(OS_MACOSX)
namespace {
const char kIsPreviewMetafileKey[] = "CrIsPreviewMetafile";
}
#endif

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

bool ToPixmap(PaintCanvas* canvas, SkPixmap* output) {
  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas->accessTopLayerPixels(&info, &row_bytes);
  if (!pixels) {
    output->reset();
    return false;
  }

  output->reset(info, pixels, row_bytes);
  return true;
}

#if defined(OS_MACOSX)
void SetIsPreviewMetafile(PaintCanvas* canvas, bool is_preview) {
  SkMetaData& meta = canvas->getMetaData();
  meta.setBool(kIsPreviewMetafileKey, is_preview);
}

bool IsPreviewMetafile(PaintCanvas* canvas) {
  bool value;
  SkMetaData& meta = canvas->getMetaData();
  if (!meta.findBool(kIsPreviewMetafileKey, &value))
    value = false;
  return value;
}
#endif

}  // namespace cc

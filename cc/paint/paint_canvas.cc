// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_canvas.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkMetaData.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

#if defined(OS_MACOSX)
namespace {
const char kIsPreviewMetafileKey[] = "CrIsPreviewMetafile";
}
#endif

namespace cc {

PaintCanvas::PaintCanvas(SkCanvas* canvas) : canvas_(canvas) {}

PaintCanvas::PaintCanvas(const SkBitmap& bitmap)
    : canvas_(new SkCanvas(bitmap)), owned_(canvas_) {}

PaintCanvas::PaintCanvas(const SkBitmap& bitmap, const SkSurfaceProps& props)
    : canvas_(new SkCanvas(bitmap, props)), owned_(canvas_) {}

PaintCanvas::~PaintCanvas() = default;

bool ToPixmap(PaintCanvas* canvas, SkPixmap* output) {
  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas->canvas_->accessTopLayerPixels(&info, &row_bytes);
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

void PaintCanvasAnnotateRectWithURL(PaintCanvas* canvas,
                                    const SkRect& rect,
                                    SkData* data) {
  SkAnnotateRectWithURL(canvas->canvas_, rect, data);
}

void PaintCanvasAnnotateNamedDestination(PaintCanvas* canvas,
                                         const SkPoint& point,
                                         SkData* data) {
  SkAnnotateNamedDestination(canvas->canvas_, point, data);
}

void PaintCanvasAnnotateLinkToDestination(PaintCanvas* canvas,
                                          const SkRect& rect,
                                          SkData* data) {
  SkAnnotateLinkToDestination(canvas->canvas_, rect, data);
}

}  // namespace cc

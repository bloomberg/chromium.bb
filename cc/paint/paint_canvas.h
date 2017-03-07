// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CANVAS_H_
#define CC_PAINT_PAINT_CANVAS_H_

#include "build/build_config.h"
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

// TODO(enne): Move all these functions into PaintCanvas.

// PaintCanvas equivalent of skia::GetWritablePixels.
CC_PAINT_EXPORT bool ToPixmap(PaintCanvas* canvas, SkPixmap* output);

// Following routines are used in print preview workflow to mark the
// preview metafile.
#if defined(OS_MACOSX)
CC_PAINT_EXPORT void SetIsPreviewMetafile(PaintCanvas* canvas, bool is_preview);
CC_PAINT_EXPORT bool IsPreviewMetafile(PaintCanvas* canvas);
#endif

}  // namespace cc

#endif  // CC_PAINT_PAINT_CANVAS_H_

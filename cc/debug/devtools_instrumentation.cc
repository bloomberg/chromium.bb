// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/devtools_instrumentation.h"

namespace cc {
namespace devtools_instrumentation {

namespace internal {
extern const char kCategory[] = TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
extern const char kCategoryFrame[] =
    TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");
extern const char kData[] = "data";
extern const char kFrameId[] = "frameId";
extern const char kLayerId[] = "layerId";
extern const char kLayerTreeId[] = "layerTreeId";
extern const char kPixelRefId[] = "pixelRefId";

extern const char kImageDecodeTask[] = "ImageDecodeTask";
extern const char kBeginFrame[] = "BeginFrame";
extern const char kNeedsBeginFrameChanged[] = "NeedsBeginFrameChanged";
extern const char kActivateLayerTree[] = "ActivateLayerTree";
extern const char kRequestMainThreadFrame[] = "RequestMainThreadFrame";
extern const char kBeginMainThreadFrame[] = "BeginMainThreadFrame";
extern const char kDrawFrame[] = "DrawFrame";
extern const char kCompositeLayers[] = "CompositeLayers";
}  // namespace internal

extern const char kPaintSetup[] = "PaintSetup";
extern const char kUpdateLayer[] = "UpdateLayer";

}  // namespace devtools_instrumentation
}  // namespace cc

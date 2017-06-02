// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/devtools_instrumentation.h"

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

ScopedImageDecodeTask::ScopedImageDecodeTask(const void* image_ptr,
                                             DecodeType decode_type,
                                             TaskType task_type)
    : decode_type_(decode_type),
      task_type_(task_type),
      start_time_(base::TimeTicks::Now()) {
  TRACE_EVENT_BEGIN1(internal::kCategory, internal::kImageDecodeTask,
                     internal::kPixelRefId,
                     reinterpret_cast<uint64_t>(image_ptr));
}

ScopedImageDecodeTask::~ScopedImageDecodeTask() {
  TRACE_EVENT_END0(internal::kCategory, internal::kImageDecodeTask);
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  switch (task_type_) {
    case kInRaster:
      switch (decode_type_) {
        case kSoftware:
          UMA_HISTOGRAM_COUNTS_1M(
              "Renderer4.ImageDecodeTaskDurationUs.Software",
              duration.InMicroseconds());
          break;
        case kGpu:
          UMA_HISTOGRAM_COUNTS_1M("Renderer4.ImageDecodeTaskDurationUs.Gpu",
                                  duration.InMicroseconds());
          break;
      }
      break;
    case kOutOfRaster:
      switch (decode_type_) {
        case kSoftware:
          UMA_HISTOGRAM_COUNTS_1M(
              "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster.Software",
              duration.InMicroseconds());
          break;
        case kGpu:
          UMA_HISTOGRAM_COUNTS_1M(
              "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster.Gpu",
              duration.InMicroseconds());
          break;
      }
      break;
  }
}

}  // namespace devtools_instrumentation
}  // namespace cc

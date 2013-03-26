// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_DEVTOOLS_INSTRUMENTATION_H_
#define CC_DEBUG_DEVTOOLS_INSTRUMENTATION_H_

#include "base/debug/trace_event.h"

namespace cc {
namespace devtools_instrumentation {

namespace internal {
const char kCategory[] = "cc,devtools";
const char kLayerId[] = "layerId";
const char kPaintLayer[] = "PaintLayer";
const char kRasterTask[] = "RasterTask";
}

struct ScopedPaintLayer {
  explicit ScopedPaintLayer(int layer_id) {
    TRACE_EVENT_BEGIN1(internal::kCategory, internal::kPaintLayer,
        internal::kLayerId, layer_id);
  }
  ~ScopedPaintLayer() {
    TRACE_EVENT_END0(internal::kCategory, internal::kPaintLayer);
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedPaintLayer);
};

struct ScopedRasterTask {
  explicit ScopedRasterTask(int layer_id) {
    TRACE_EVENT_BEGIN1(internal::kCategory, internal::kRasterTask,
        internal::kLayerId, layer_id);
  }
  ~ScopedRasterTask() {
    TRACE_EVENT_END0(internal::kCategory, internal::kRasterTask);
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedRasterTask);
};

struct ScopedLayerObjectTracker
    : public base::debug::TraceScopedTrackableObject<int> {
  explicit ScopedLayerObjectTracker(int layer_id)
      : base::debug::TraceScopedTrackableObject<int>(
            internal::kCategory,
            internal::kLayerId,
            layer_id) {
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerObjectTracker);
};

}  // namespace devtools_instrumentation
}  // namespace cc

#endif  // CC_DEBUG_DEVTOOLS_INSTRUMENTATION_H_


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
const char kLayerTreeId[] = "layerTreeId";
}

const char kPaintLayer[] = "PaintLayer";
const char kRasterTask[] = "RasterTask";
const char kImageDecodeTask[] = "ImageDecodeTask";
const char kPaintSetup[] = "PaintSetup";
const char kUpdateLayer[] = "UpdateLayer";

class ScopedLayerTask {
 public:
  ScopedLayerTask(const char* event_name, int layer_id)
    : event_name_(event_name) {
    TRACE_EVENT_BEGIN1(internal::kCategory, event_name_,
        internal::kLayerId, layer_id);
  }
  ~ScopedLayerTask() {
    TRACE_EVENT_END0(internal::kCategory, event_name_);
  }
 private:
  const char* event_name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerTask);
};

class ScopedLayerTreeTask {
 public:
  ScopedLayerTreeTask(const char* event_name,
                      int layer_id,
                      uint64 tree_id)
    : event_name_(event_name) {
    TRACE_EVENT_BEGIN2(internal::kCategory, event_name_,
        internal::kLayerId, layer_id, internal::kLayerTreeId, tree_id);
  }
  ~ScopedLayerTreeTask() {
    TRACE_EVENT_END0(internal::kCategory, event_name_);
  }
 private:
  const char* event_name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerTreeTask);
};

struct ScopedLayerObjectTracker
    : public base::debug::TraceScopedTrackableObject<int> {
  explicit ScopedLayerObjectTracker(int layer_id)
      : base::debug::TraceScopedTrackableObject<int>(
            internal::kCategory,
            internal::kLayerId,
            layer_id) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedLayerObjectTracker);
};

}  // namespace devtools_instrumentation
}  // namespace cc

#endif  // CC_DEBUG_DEVTOOLS_INSTRUMENTATION_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_FRAME_VIEWER_INSTRUMENTATION_H_
#define CC_TILES_FRAME_VIEWER_INSTRUMENTATION_H_

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/tiles/tile_priority.h"

namespace cc {
namespace frame_viewer_instrumentation {

constexpr const char* CategoryLayerTree() {
  // Declared as a constexpr function to have an external linkage and to be
  // known at compile-time.
  return TRACE_DISABLED_BY_DEFAULT("cc.debug") "," TRACE_DISABLED_BY_DEFAULT(
      "viz.quads") "," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.layers");
}

class ScopedAnalyzeTask {
 public:
  ScopedAnalyzeTask(const void* tile_id,
                    TileResolution tile_resolution,
                    int source_frame_number,
                    int layer_id);
  ~ScopedAnalyzeTask();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedAnalyzeTask);
};

class ScopedRasterTask {
 public:
  ScopedRasterTask(const void* tile_id,
                   TileResolution tile_resolution,
                   int source_frame_number,
                   int layer_id);
  ~ScopedRasterTask();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedRasterTask);
};

bool IsTracingLayerTreeSnapshots();

}  // namespace frame_viewer_instrumentation
}  // namespace cc

#endif  // CC_TILES_FRAME_VIEWER_INSTRUMENTATION_H_

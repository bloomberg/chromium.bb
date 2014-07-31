// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_FRAME_VIEWER_INSTRUMENTATION_H_
#define CC_DEBUG_FRAME_VIEWER_INSTRUMENTATION_H_

#include "base/debug/trace_event.h"
#include "cc/resources/tile.h"

namespace cc {
namespace frame_viewer_instrumentation {
namespace internal {

const char kCategory[] = "cc";
const char kTileData[] = "tileData";
const char kLayerId[] = "layerId";
const char kTileId[] = "tileId";
const char kTileResolution[] = "tileResolution";
const char kSourceFrameNumber[] = "sourceFrameNumber";
const char kRasterMode[] = "rasterMode";

const char kAnalyzeTask[] = "AnalyzeTask";
const char kRasterTask[] = "RasterTask";

scoped_refptr<base::debug::ConvertableToTraceFormat> TileDataAsValue(
    const void* tile_id,
    TileResolution tile_resolution,
    int source_frame_number,
    int layer_id) {
  scoped_refptr<base::debug::TracedValue> res(new base::debug::TracedValue());
  TracedValue::SetIDRef(tile_id, res, internal::kTileId);
  res->SetString(internal::kTileResolution,
                 TileResolutionToString(tile_resolution));
  res->SetInteger(internal::kSourceFrameNumber, source_frame_number);
  res->SetInteger(internal::kLayerId, layer_id);
  return res;
}

}  // namespace internal

class ScopedAnalyzeTask {
 public:
  ScopedAnalyzeTask(const void* tile_id,
                    TileResolution tile_resolution,
                    int source_frame_number,
                    int layer_id) {
    TRACE_EVENT_BEGIN1(
        internal::kCategory,
        internal::kAnalyzeTask,
        internal::kTileData,
        internal::TileDataAsValue(
            tile_id, tile_resolution, source_frame_number, layer_id));
  }
  ~ScopedAnalyzeTask() {
    TRACE_EVENT_END0(internal::kCategory, internal::kAnalyzeTask);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedAnalyzeTask);
};

class ScopedRasterTask {
 public:
  ScopedRasterTask(const void* tile_id,
                   TileResolution tile_resolution,
                   int source_frame_number,
                   int layer_id,
                   RasterMode raster_mode) {
    TRACE_EVENT_BEGIN2(
        internal::kCategory,
        internal::kRasterTask,
        internal::kTileData,
        internal::TileDataAsValue(
            tile_id, tile_resolution, source_frame_number, layer_id),
        internal::kRasterMode,
        RasterModeToString(raster_mode));
  }
  ~ScopedRasterTask() {
    TRACE_EVENT_END0(internal::kCategory, internal::kRasterTask);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedRasterTask);
};

}  // namespace frame_viewer_instrumentation
}  // namespace cc

#endif  // CC_DEBUG_FRAME_VIEWER_INSTRUMENTATION_H_

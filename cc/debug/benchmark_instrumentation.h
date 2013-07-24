// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_
#define CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_

#include "base/debug/trace_event.h"

namespace cc {
namespace benchmark_instrumentation {
// Please do not change the string constants in this file (or the TRACE_EVENT
// calls that use them) without updating
// tools/perf/measurements/rasterize_and_record_benchmark.py accordingly.
// The benchmark searches for events and their arguments by name.
const char kCategory[] = "cc,benchmark";
const char kSourceFrameNumber[] = "source_frame_number";
const char kData[] = "data";
const char kWidth[] = "width";
const char kHeight[] = "height";
const char kNumPixelsRasterized[] = "num_pixels_rasterized";
const char kLayerTreeHostUpdateLayers[] = "LayerTreeHost::UpdateLayers";
const char kPictureLayerUpdate[] = "PictureLayer::Update";
const char kRunRasterOnThread[] = "RasterWorkerPoolTaskImpl::RunRasterOnThread";
const char kRecordLoop[] = "RecordLoop";
const char kRasterLoop[] = "RasterLoop";
const char kPictureRecord[] = "Picture::Record";
const char kPictureRaster[] = "Picture::Raster";
}  // namespace benchmark_instrumentation
}  // namespace cc

#endif  // CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_

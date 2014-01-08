// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rasterize_and_record_benchmark_impl.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/values.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/rect.h"

namespace cc {

namespace {

const int kDefaultRasterizeRepeatCount = 100;

base::TimeTicks Now() {
  return base::TimeTicks::IsThreadNowSupported()
             ? base::TimeTicks::ThreadNow()
             : base::TimeTicks::HighResNow();
}

}  // namespace

RasterizeAndRecordBenchmarkImpl::RasterizeAndRecordBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop,
    base::Value* value,
    const MicroBenchmarkImpl::DoneCallback& callback)
    : MicroBenchmarkImpl(callback, origin_loop),
      rasterize_repeat_count_(kDefaultRasterizeRepeatCount) {
  base::DictionaryValue* settings = NULL;
  value->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("rasterize_repeat_count"))
    settings->GetInteger("rasterize_repeat_count", &rasterize_repeat_count_);
}

RasterizeAndRecordBenchmarkImpl::~RasterizeAndRecordBenchmarkImpl() {}

void RasterizeAndRecordBenchmarkImpl::DidCompleteCommit(
    LayerTreeHostImpl* host) {
  LayerTreeHostCommon::CallFunctionForSubtree(
      host->RootLayer(),
      base::Bind(&RasterizeAndRecordBenchmarkImpl::Run,
                 base::Unretained(this)));

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger("pixels_rasterized", rasterize_results_.pixels_rasterized);
  result->SetDouble("rasterize_time_ms",
                    rasterize_results_.total_best_time.InMillisecondsF());

  NotifyDone(result.PassAs<base::Value>());
}

void RasterizeAndRecordBenchmarkImpl::Run(LayerImpl* layer) {
  layer->RunMicroBenchmark(this);
}

void RasterizeAndRecordBenchmarkImpl::RunOnLayer(PictureLayerImpl* layer) {
  if (layer->visible_content_rect().IsEmpty())
    return;

  PictureLayerTilingSet tiling_set(layer, layer->content_bounds());

  PictureLayerTiling* tiling = tiling_set.AddTiling(layer->contents_scale_x());
  tiling->CreateAllTilesForTesting();
  for (PictureLayerTiling::CoverageIterator it(
           tiling, layer->contents_scale_x(), layer->visible_content_rect());
       it;
       ++it) {
    DCHECK(*it);

    PicturePileImpl* picture_pile = (*it)->picture_pile();
    gfx::Rect content_rect = (*it)->content_rect();
    float contents_scale = (*it)->contents_scale();

    base::TimeDelta min_time =
        base::TimeDelta::FromInternalValue(std::numeric_limits<int64>::max());
    for (int i = 0; i < rasterize_repeat_count_; ++i) {
      SkBitmap bitmap;
      bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                       content_rect.width(),
                       content_rect.height());
      bitmap.allocPixels();

      SkBitmapDevice device(bitmap);
      SkCanvas canvas(&device);
      PicturePileImpl::Analysis analysis;

      base::TimeTicks start = Now();
      picture_pile->AnalyzeInRect(
          content_rect, contents_scale, &analysis, NULL);
      picture_pile->RasterToBitmap(&canvas, content_rect, contents_scale, NULL);
      base::TimeTicks end = Now();
      base::TimeDelta duration = end - start;
      if (duration < min_time)
        min_time = duration;
    }

    rasterize_results_.pixels_rasterized +=
        content_rect.width() * content_rect.height();
    rasterize_results_.total_best_time += min_time;
  }
}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::RasterizeResults()
    : pixels_rasterized(0) {}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::~RasterizeResults() {}

}  // namespace cc

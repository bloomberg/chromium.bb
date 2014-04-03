// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rasterize_and_record_benchmark.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/values.h"
#include "cc/debug/rasterize_and_record_benchmark_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "ui/gfx/rect.h"

namespace cc {

namespace {

const int kDefaultRecordRepeatCount = 100;

base::TimeTicks Now() {
  return base::TimeTicks::IsThreadNowSupported()
             ? base::TimeTicks::ThreadNow()
             : base::TimeTicks::HighResNow();
}

}  // namespace

RasterizeAndRecordBenchmark::RasterizeAndRecordBenchmark(
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback)
    : MicroBenchmark(callback),
      record_repeat_count_(kDefaultRecordRepeatCount),
      settings_(value.Pass()),
      main_thread_benchmark_done_(false),
      host_(NULL),
      weak_ptr_factory_(this) {
  base::DictionaryValue* settings = NULL;
  settings_->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("record_repeat_count"))
    settings->GetInteger("record_repeat_count", &record_repeat_count_);
}

RasterizeAndRecordBenchmark::~RasterizeAndRecordBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterizeAndRecordBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  host_ = host;
  LayerTreeHostCommon::CallFunctionForSubtree(
      host->root_layer(),
      base::Bind(&RasterizeAndRecordBenchmark::Run, base::Unretained(this)));

  DCHECK(!results_.get());
  results_ = make_scoped_ptr(new base::DictionaryValue);
  results_->SetInteger("pixels_recorded", record_results_.pixels_recorded);
  results_->SetDouble("record_time_ms",
                      record_results_.total_best_time.InMillisecondsF());
  main_thread_benchmark_done_ = true;
}

void RasterizeAndRecordBenchmark::RecordRasterResults(
    scoped_ptr<base::Value> results_value) {
  DCHECK(main_thread_benchmark_done_);

  base::DictionaryValue* results = NULL;
  results_value->GetAsDictionary(&results);
  DCHECK(results);

  results_->MergeDictionary(results);

  NotifyDone(results_.PassAs<base::Value>());
}

scoped_ptr<MicroBenchmarkImpl> RasterizeAndRecordBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop) {
  return scoped_ptr<MicroBenchmarkImpl>(new RasterizeAndRecordBenchmarkImpl(
      origin_loop,
      settings_.get(),
      base::Bind(&RasterizeAndRecordBenchmark::RecordRasterResults,
                 weak_ptr_factory_.GetWeakPtr())));
}

void RasterizeAndRecordBenchmark::Run(Layer* layer) {
  layer->RunMicroBenchmark(this);
}

void RasterizeAndRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  ContentLayerClient* painter = layer->client();
  gfx::Size content_bounds = layer->content_bounds();

  DCHECK(host_);
  gfx::Size tile_grid_size = host_->settings().default_tile_size;

  SkTileGridPicture::TileGridInfo tile_grid_info;
  PicturePileBase::ComputeTileGridInfo(tile_grid_size, &tile_grid_info);

  gfx::Rect visible_content_rect = gfx::ScaleToEnclosingRect(
      layer->visible_content_rect(), 1.f / layer->contents_scale_x());
  if (visible_content_rect.IsEmpty())
    return;


  base::TimeDelta min_time = base::TimeDelta::Max();
  for (int i = 0; i < record_repeat_count_; ++i) {
    base::TimeTicks start = Now();
    scoped_refptr<Picture> picture = Picture::Create(
        visible_content_rect, painter, tile_grid_info, false, 0);
    base::TimeTicks end = Now();
    base::TimeDelta duration = end - start;
    if (duration < min_time)
      min_time = duration;
  }

  record_results_.pixels_recorded +=
      visible_content_rect.width() * visible_content_rect.height();
  record_results_.total_best_time += min_time;
}

RasterizeAndRecordBenchmark::RecordResults::RecordResults()
    : pixels_recorded(0) {}

RasterizeAndRecordBenchmark::RecordResults::~RecordResults() {}

}  // namespace cc

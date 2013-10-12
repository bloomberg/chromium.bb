// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/values.h"
#include "cc/debug/picture_record_benchmark.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "ui/gfx/rect.h"

namespace cc {

namespace {

const int kDimensions[] = {1, 250, 500, 750, 1000};
const int kDimensionsCount = arraysize(kDimensions);
const int kPositionIncrement = 100;

const int kTileGridSize = 512;
const int kTileGridBorder = 1;

}  // namespace

PictureRecordBenchmark::PictureRecordBenchmark(
    const MicroBenchmark::DoneCallback& callback)
    : MicroBenchmark(callback) {}

PictureRecordBenchmark::~PictureRecordBenchmark() {}

void PictureRecordBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  LayerTreeHostCommon::CallFunctionForSubtree(
      host->root_layer(),
      base::Bind(&PictureRecordBenchmark::Run, base::Unretained(this)));

  scoped_ptr<base::ListValue> results(new base::ListValue());
  for (std::map<unsigned, TotalTime>::iterator it = times_.begin();
       it != times_.end();
       ++it) {
    unsigned area = it->first;
    base::TimeDelta total_time = it->second.first;
    unsigned total_count = it->second.second;

    double average_time = 0.0;
    if (total_count > 0)
      average_time = total_time.InMillisecondsF() / total_count;

    scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger("area", area);
    result->SetDouble("time_ms", average_time);

    results->Append(result.release());
  }

  NotifyDone(results.PassAs<base::Value>());
}

void PictureRecordBenchmark::Run(Layer* layer) {
  layer->RunMicroBenchmark(this);
}

void PictureRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  ContentLayerClient* painter = layer->client();
  gfx::Size content_bounds = layer->content_bounds();

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval.set(kTileGridSize - 2 * kTileGridBorder,
                                   kTileGridSize - 2 * kTileGridBorder);
  tile_grid_info.fMargin.set(kTileGridBorder, kTileGridBorder);
  tile_grid_info.fOffset.set(-kTileGridBorder, -kTileGridBorder);

  for (int i = 0; i < kDimensionsCount; ++i) {
    int dimensions = kDimensions[i];
    unsigned area = dimensions * dimensions;
    for (int y = 0; y < content_bounds.height(); y += kPositionIncrement) {
      for (int x = 0; x < content_bounds.width(); x += kPositionIncrement) {
        gfx::Rect rect = gfx::Rect(x, y, dimensions, dimensions);

        base::TimeTicks start = base::TimeTicks::HighResNow();

        scoped_refptr<Picture> picture = Picture::Create(rect);
        picture->Record(painter, tile_grid_info);

        base::TimeTicks end = base::TimeTicks::HighResNow();
        base::TimeDelta duration = end - start;
        TotalTime& total_time = times_[area];
        total_time.first += duration;
        total_time.second++;
      }
    }
  }
}

}  // namespace cc

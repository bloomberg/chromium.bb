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
#include "cc/resources/raster_worker_pool.h"
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

class BenchmarkRasterTask : public Task {
 public:
  BenchmarkRasterTask(PicturePileImpl* picture_pile,
                      const gfx::Rect& content_rect,
                      float contents_scale,
                      size_t repeat_count)
      : picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        repeat_count_(repeat_count),
        is_solid_color_(false),
        best_time_(base::TimeDelta::Max()) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    PicturePileImpl* picture_pile = picture_pile_->GetCloneForDrawingOnThread(
        RasterWorkerPool::GetPictureCloneIndexForCurrentThread());

    for (size_t i = 0; i < repeat_count_; ++i) {
      SkBitmap bitmap;
      bitmap.allocPixels(SkImageInfo::MakeN32Premul(content_rect_.width(),
                                                    content_rect_.height()));
      SkCanvas canvas(bitmap);
      PicturePileImpl::Analysis analysis;

      base::TimeTicks start = Now();
      picture_pile->AnalyzeInRect(
          content_rect_, contents_scale_, &analysis, NULL);
      picture_pile->RasterToBitmap(
          &canvas, content_rect_, contents_scale_, NULL);
      base::TimeTicks end = Now();
      base::TimeDelta duration = end - start;
      if (duration < best_time_)
        best_time_ = duration;

      is_solid_color_ = analysis.is_solid_color;
    }
  }

  bool IsSolidColor() const { return is_solid_color_; }
  base::TimeDelta GetBestTime() const { return best_time_; }

 private:
  virtual ~BenchmarkRasterTask() {}

  PicturePileImpl* picture_pile_;
  gfx::Rect content_rect_;
  float contents_scale_;
  size_t repeat_count_;
  bool is_solid_color_;
  base::TimeDelta best_time_;
};

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
  result->SetDouble("rasterize_time_ms",
                    rasterize_results_.total_best_time.InMillisecondsF());
  result->SetInteger("pixels_rasterized", rasterize_results_.pixels_rasterized);
  result->SetInteger("pixels_rasterized_with_non_solid_color",
                     rasterize_results_.pixels_rasterized_with_non_solid_color);
  result->SetInteger("pixels_rasterized_as_opaque",
                     rasterize_results_.pixels_rasterized_as_opaque);
  result->SetInteger("total_layers", rasterize_results_.total_layers);
  result->SetInteger("total_picture_layers",
                     rasterize_results_.total_picture_layers);
  result->SetInteger("total_picture_layers_with_no_content",
                     rasterize_results_.total_picture_layers_with_no_content);
  result->SetInteger("total_picture_layers_off_screen",
                     rasterize_results_.total_picture_layers_off_screen);

  NotifyDone(result.PassAs<base::Value>());
}

void RasterizeAndRecordBenchmarkImpl::Run(LayerImpl* layer) {
  rasterize_results_.total_layers++;
  layer->RunMicroBenchmark(this);
}

void RasterizeAndRecordBenchmarkImpl::RunOnLayer(PictureLayerImpl* layer) {
  rasterize_results_.total_picture_layers++;
  if (!layer->DrawsContent()) {
    rasterize_results_.total_picture_layers_with_no_content++;
    return;
  }
  if (layer->visible_content_rect().IsEmpty()) {
    rasterize_results_.total_picture_layers_off_screen++;
    return;
  }

  TaskGraphRunner* task_graph_runner = RasterWorkerPool::GetTaskGraphRunner();
  DCHECK(task_graph_runner);

  if (!task_namespace_.IsValid())
    task_namespace_ = task_graph_runner->GetNamespaceToken();

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

    scoped_refptr<BenchmarkRasterTask> benchmark_raster_task(
        new BenchmarkRasterTask(picture_pile,
                                content_rect,
                                contents_scale,
                                rasterize_repeat_count_));

    TaskGraph graph;

    graph.nodes.push_back(
        TaskGraph::Node(benchmark_raster_task,
                        RasterWorkerPool::kBenchmarkRasterTaskPriority,
                        0u));

    task_graph_runner->ScheduleTasks(task_namespace_, &graph);
    task_graph_runner->WaitForTasksToFinishRunning(task_namespace_);

    Task::Vector completed_tasks;
    task_graph_runner->CollectCompletedTasks(task_namespace_, &completed_tasks);
    DCHECK_EQ(1u, completed_tasks.size());
    DCHECK_EQ(completed_tasks[0], benchmark_raster_task);

    int tile_size = content_rect.width() * content_rect.height();
    base::TimeDelta min_time = benchmark_raster_task->GetBestTime();
    bool is_solid_color = benchmark_raster_task->IsSolidColor();

    if (layer->contents_opaque())
      rasterize_results_.pixels_rasterized_as_opaque += tile_size;

    if (!is_solid_color) {
      rasterize_results_.pixels_rasterized_with_non_solid_color += tile_size;
    }

    rasterize_results_.pixels_rasterized += tile_size;
    rasterize_results_.total_best_time += min_time;
  }
}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::RasterizeResults()
    : pixels_rasterized(0),
      pixels_rasterized_with_non_solid_color(0),
      pixels_rasterized_as_opaque(0),
      total_layers(0),
      total_picture_layers(0),
      total_picture_layers_with_no_content(0),
      total_picture_layers_off_screen(0) {}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::~RasterizeResults() {}

}  // namespace cc

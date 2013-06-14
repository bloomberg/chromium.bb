// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/paint_simplifier.h"

namespace cc {

namespace {

class DisableLCDTextFilter : public SkDrawFilter {
 public:
  // SkDrawFilter interface.
  virtual bool filter(SkPaint* paint, SkDrawFilter::Type type) OVERRIDE {
    if (type != SkDrawFilter::kText_Type)
      return true;

    paint->setLCDRenderText(false);
    return true;
  }
};

void Noop() {}

class RootWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  RootWorkerPoolTaskImpl(const base::Closure& callback,
                         const base::Closure& reply)
      : callback_(callback), reply_(reply) {}

  explicit RootWorkerPoolTaskImpl(
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        callback_(base::Bind(&Noop)),
        reply_(base::Bind(&Noop)) {}

  RootWorkerPoolTaskImpl(const base::Closure& callback,
                         internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        callback_(callback),
        reply_(base::Bind(&Noop)) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    callback_.Run();
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run();
  }

 private:
  virtual ~RootWorkerPoolTaskImpl() {}

  const base::Closure callback_;
  const base::Closure reply_;

  DISALLOW_COPY_AND_ASSIGN(RootWorkerPoolTaskImpl);
};

class RasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(const Resource* resource,
                           PicturePileImpl* picture_pile,
                           gfx::Rect content_rect,
                           float contents_scale,
                           RasterMode raster_mode,
                           bool use_color_estimator,
                           const RasterTaskMetadata& metadata,
                           RenderingStatsInstrumentation* rendering_stats,
                           const RasterWorkerPool::RasterTask::Reply& reply,
                           internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
        use_color_estimator_(use_color_estimator),
        metadata_(metadata),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  void RunAnalysisOnThread(unsigned thread_index) {
    TRACE_EVENT1("cc",
                 "RasterWorkerPoolTaskImpl::RunAnalysisOnThread",
                 "metadata",
                 TracedValue::FromValue(metadata_.AsValue().release()));

    DCHECK(picture_pile_.get());
    DCHECK(rendering_stats_);

    PicturePileImpl* picture_clone =
        picture_pile_->GetCloneForDrawingOnThread(thread_index);

    DCHECK(picture_clone);

    base::TimeTicks start_time = rendering_stats_->StartRecording();
    picture_clone->AnalyzeInRect(content_rect_, contents_scale_, &analysis_);
    base::TimeDelta duration = rendering_stats_->EndRecording(start_time);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);
    rendering_stats_->AddTileAnalysisResult(duration,
                                            analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= use_color_estimator_;
  }

  bool RunRasterOnThread(SkDevice* device, unsigned thread_index) {
    TRACE_EVENT1(
        "cc", "RasterWorkerPoolTaskImpl::RunRasterOnThread",
        "metadata", TracedValue::FromValue(metadata_.AsValue().release()));
    devtools_instrumentation::ScopedLayerTask raster_task(
        devtools_instrumentation::kRasterTask, metadata_.layer_id);

    DCHECK(picture_pile_.get());
    DCHECK(device);

    if (analysis_.is_solid_color)
      return false;

    PicturePileImpl* picture_clone =
        picture_pile_->GetCloneForDrawingOnThread(thread_index);

    SkCanvas canvas(device);

    skia::RefPtr<SkDrawFilter> draw_filter;
    switch (raster_mode_) {
      case LOW_QUALITY_RASTER_MODE:
        draw_filter = skia::AdoptRef(new skia::PaintSimplifier);
        break;
      case HIGH_QUALITY_NO_LCD_RASTER_MODE:
        draw_filter = skia::AdoptRef(new DisableLCDTextFilter);
        break;
      case HIGH_QUALITY_RASTER_MODE:
        break;
      case NUM_RASTER_MODES:
      default:
        NOTREACHED();
    }

    canvas.setDrawFilter(draw_filter.get());

    if (rendering_stats_->record_rendering_stats()) {
      PicturePileImpl::RasterStats raster_stats;
      picture_clone->RasterToBitmap(
          &canvas, content_rect_, contents_scale_, &raster_stats);
      rendering_stats_->AddRaster(
          raster_stats.total_rasterize_time,
          raster_stats.best_rasterize_time,
          raster_stats.total_pixels_rasterized,
          metadata_.is_tile_in_pending_tree_now_bin);

      HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.PictureRasterTimeUS",
          raster_stats.total_rasterize_time.InMicroseconds(),
          0,
          100000,
          100);
    } else {
      picture_clone->RasterToBitmap(
          &canvas, content_rect_, contents_scale_, NULL);
    }
    return true;
  }

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnThread(SkDevice* device, unsigned thread_index) OVERRIDE {
    RunAnalysisOnThread(thread_index);
    return RunRasterOnThread(device, thread_index);
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run(analysis_, !HasFinishedRunning());
  }

 protected:
  virtual ~RasterWorkerPoolTaskImpl() {}

 private:
  PicturePileImpl::Analysis analysis_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect content_rect_;
  float contents_scale_;
  RasterMode raster_mode_;
  bool use_color_estimator_;
  RasterTaskMetadata metadata_;
  RenderingStatsInstrumentation* rendering_stats_;
  const RasterWorkerPool::RasterTask::Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPoolTaskImpl);
};

class ImageDecodeWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  ImageDecodeWorkerPoolTaskImpl(skia::LazyPixelRef* pixel_ref,
                                int layer_id,
                                RenderingStatsInstrumentation* rendering_stats,
                                const RasterWorkerPool::Task::Reply& reply)
      : pixel_ref_(pixel_ref),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnThread");
    devtools_instrumentation::ScopedLayerTask image_decode_task(
        devtools_instrumentation::kImageDecodeTask, layer_id_);
    base::TimeTicks start_time = rendering_stats_->StartRecording();
    pixel_ref_->Decode();
    base::TimeDelta duration = rendering_stats_->EndRecording(start_time);
    rendering_stats_->AddDeferredImageDecode(duration);
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run();
  }

 protected:
  virtual ~ImageDecodeWorkerPoolTaskImpl() {}

 private:
  skia::LazyPixelRef* pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const RasterWorkerPool::Task::Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeWorkerPoolTaskImpl);
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

}  // namespace

namespace internal {

RasterWorkerPoolTask::RasterWorkerPoolTask(
    const Resource* resource,
    WorkerPoolTask::TaskVector* dependencies)
    : did_run_(false),
      did_complete_(false),
      resource_(resource) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {
}

void RasterWorkerPoolTask::DidRun() {
  DCHECK(!did_run_);
  did_run_ = true;
}

bool RasterWorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

void RasterWorkerPoolTask::DidComplete() {
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool RasterWorkerPoolTask::HasCompleted() const {
  return did_complete_;
}

}  // namespace internal

scoped_ptr<base::Value> RasterTaskMetadata::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("tile_id", TracedValue::CreateIDRef(tile_id).release());
  res->SetBoolean("is_tile_in_pending_tree_now_bin",
                  is_tile_in_pending_tree_now_bin);
  res->Set("resolution", TileResolutionAsValue(tile_resolution).release());
  res->SetInteger("source_frame_number", source_frame_number);
  return res.PassAs<base::Value>();
}

RasterWorkerPool::Task::Set::Set() {
}

RasterWorkerPool::Task::Set::~Set() {
}

void RasterWorkerPool::Task::Set::Insert(const Task& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::Task::Task() {
}

RasterWorkerPool::Task::Task(internal::WorkerPoolTask* internal)
    : internal_(internal) {
}

RasterWorkerPool::Task::~Task() {
}

void RasterWorkerPool::Task::Reset() {
  internal_ = NULL;
}

RasterWorkerPool::RasterTask::Queue::Queue() {
}

RasterWorkerPool::RasterTask::Queue::~Queue() {
}

void RasterWorkerPool::RasterTask::Queue::Append(
    const RasterTask& task, bool required_for_activation) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
  if (required_for_activation)
    tasks_required_for_activation_.insert(task.internal_.get());
}

RasterWorkerPool::RasterTask::RasterTask() {
}

RasterWorkerPool::RasterTask::RasterTask(
    internal::RasterWorkerPoolTask* internal)
    : internal_(internal) {
}

void RasterWorkerPool::RasterTask::Reset() {
  internal_ = NULL;
}

RasterWorkerPool::RasterTask::~RasterTask() {
}

RasterWorkerPool::RootTask::RootTask() {
}

RasterWorkerPool::RootTask::RootTask(
    internal::WorkerPoolTask::TaskVector* dependencies)
    : internal_(new RootWorkerPoolTaskImpl(dependencies)) {
}

RasterWorkerPool::RootTask::RootTask(
    const base::Closure& callback,
    internal::WorkerPoolTask::TaskVector* dependencies)
    : internal_(new RootWorkerPoolTaskImpl(callback, dependencies)) {
}

RasterWorkerPool::RootTask::~RootTask() {
}

// static
RasterWorkerPool::RasterTask RasterWorkerPool::CreateRasterTask(
    const Resource* resource,
    PicturePileImpl* picture_pile,
    gfx::Rect content_rect,
    float contents_scale,
    RasterMode raster_mode,
    bool use_color_estimator,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* rendering_stats,
    const RasterTask::Reply& reply,
    Task::Set& dependencies) {
  return RasterTask(new RasterWorkerPoolTaskImpl(resource,
                                                 picture_pile,
                                                 content_rect,
                                                 contents_scale,
                                                 raster_mode,
                                                 use_color_estimator,
                                                 metadata,
                                                 rendering_stats,
                                                 reply,
                                                 &dependencies.tasks_));
}

// static
RasterWorkerPool::Task RasterWorkerPool::CreateImageDecodeTask(
    skia::LazyPixelRef* pixel_ref,
    int layer_id,
    RenderingStatsInstrumentation* stats_instrumentation,
    const Task::Reply& reply) {
  return Task(new ImageDecodeWorkerPoolTaskImpl(pixel_ref,
                                                layer_id,
                                                stats_instrumentation,
                                                reply));
}

RasterWorkerPool::RasterWorkerPool(ResourceProvider* resource_provider,
                                   size_t num_threads)
    : WorkerPool(num_threads, kWorkerThreadNamePrefix),
      client_(NULL),
      resource_provider_(resource_provider) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void RasterWorkerPool::Shutdown() {
  raster_tasks_.clear();
  WorkerPool::Shutdown();
}

void RasterWorkerPool::SetRasterTasks(RasterTask::Queue* queue) {
  raster_tasks_.swap(queue->tasks_);
  raster_tasks_required_for_activation_.swap(
      queue->tasks_required_for_activation_);
}

void RasterWorkerPool::ScheduleRasterTasks(const RootTask& root) {
  scoped_refptr<internal::WorkerPoolTask> new_root(root.internal_);

  TaskGraph graph;
  BuildTaskGraph(new_root.get(), &graph);
  WorkerPool::SetTaskGraph(&graph);

  root_.swap(new_root);
}

bool RasterWorkerPool::IsRasterTaskRequiredForActivation(
    internal::RasterWorkerPoolTask* task) const {
  return
      raster_tasks_required_for_activation_.find(task) !=
      raster_tasks_required_for_activation_.end();
}

}  // namespace cc

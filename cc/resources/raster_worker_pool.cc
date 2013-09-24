// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

namespace {

// Subclass of Allocator that takes a suitably allocated pointer and uses
// it as the pixel memory for the bitmap.
class IdentityAllocator : public SkBitmap::Allocator {
 public:
  explicit IdentityAllocator(void* buffer) : buffer_(buffer) {}
  virtual bool allocPixelRef(SkBitmap* dst, SkColorTable*) OVERRIDE {
    dst->setPixels(buffer_);
    return true;
  }
 private:
  void* buffer_;
};

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

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

class RasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(const Resource* resource,
                           PicturePileImpl* picture_pile,
                           gfx::Rect content_rect,
                           float contents_scale,
                           RasterMode raster_mode,
                           bool is_tile_in_pending_tree_now_bin,
                           TileResolution tile_resolution,
                           int layer_id,
                           const void* tile_id,
                           int source_frame_number,
                           RenderingStatsInstrumentation* rendering_stats,
                           const RasterWorkerPool::RasterTask::Reply& reply,
                           TaskVector* dependencies)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
        is_tile_in_pending_tree_now_bin_(is_tile_in_pending_tree_now_bin),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        tile_id_(tile_id),
        source_frame_number_(source_frame_number),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  void RunAnalysisOnThread(unsigned thread_index) {
    TRACE_EVENT1("cc",
                 "RasterWorkerPoolTaskImpl::RunAnalysisOnThread",
                 "data",
                 TracedValue::FromValue(DataAsValue().release()));

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
    rendering_stats_->AddAnalysisResult(duration, analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  bool RunRasterOnThread(unsigned thread_index,
                         void* buffer,
                         gfx::Size size,
                         int stride) {
    TRACE_EVENT2(
        benchmark_instrumentation::kCategory,
        benchmark_instrumentation::kRunRasterOnThread,
        benchmark_instrumentation::kData,
        TracedValue::FromValue(DataAsValue().release()),
        "raster_mode",
        TracedValue::FromValue(RasterModeAsValue(raster_mode_).release()));

    devtools_instrumentation::ScopedLayerTask raster_task(
        devtools_instrumentation::kRasterTask, layer_id_);

    DCHECK(picture_pile_.get());
    DCHECK(buffer);

    if (analysis_.is_solid_color)
      return false;

    PicturePileImpl* picture_clone =
        picture_pile_->GetCloneForDrawingOnThread(thread_index);

    SkBitmap bitmap;
    switch (resource()->format()) {
      case RGBA_4444:
        // Use the default stride if we will eventually convert this
        // bitmap to 4444.
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         size.width(),
                         size.height());
        bitmap.allocPixels();
        break;
      case RGBA_8888:
      case BGRA_8888:
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         size.width(),
                         size.height(),
                         stride);
        bitmap.setPixels(buffer);
        break;
      case LUMINANCE_8:
      case RGB_565:
        NOTREACHED();
        break;
    }

    SkBitmapDevice device(bitmap);
    SkCanvas canvas(&device);
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
          is_tile_in_pending_tree_now_bin_);

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

    ChangeBitmapConfigIfNeeded(bitmap, buffer);

    return true;
  }

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnWorkerThread(unsigned thread_index,
                                 void* buffer,
                                 gfx::Size size,
                                 int stride)
      OVERRIDE {
    RunAnalysisOnThread(thread_index);
    return RunRasterOnThread(thread_index, buffer, size, stride);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(analysis_, !HasFinishedRunning() || WasCanceled());
  }

 protected:
  virtual ~RasterWorkerPoolTaskImpl() {}

 private:
  scoped_ptr<base::Value> DataAsValue() const {
    scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
    res->Set("tile_id", TracedValue::CreateIDRef(tile_id_).release());
    res->SetBoolean("is_tile_in_pending_tree_now_bin",
                    is_tile_in_pending_tree_now_bin_);
    res->Set("resolution", TileResolutionAsValue(tile_resolution_).release());
    res->SetInteger("source_frame_number", source_frame_number_);
    res->SetInteger("layer_id", layer_id_);
    return res.PassAs<base::Value>();
  }

  void ChangeBitmapConfigIfNeeded(const SkBitmap& bitmap,
                                  void* buffer) {
    TRACE_EVENT0("cc", "RasterWorkerPoolTaskImpl::ChangeBitmapConfigIfNeeded");
    SkBitmap::Config config = SkBitmapConfigFromFormat(
        resource()->format());
    if (bitmap.getConfig() != config) {
      SkBitmap bitmap_dest;
      IdentityAllocator allocator(buffer);
      bitmap.copyTo(&bitmap_dest, config, &allocator);
      // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
      // bitmap data. This check will be removed once crbug.com/293728 is fixed.
      CHECK_EQ(0u, bitmap_dest.rowBytes() % 4);
    }
  }

  PicturePileImpl::Analysis analysis_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect content_rect_;
  float contents_scale_;
  RasterMode raster_mode_;
  bool is_tile_in_pending_tree_now_bin_;
  TileResolution tile_resolution_;
  int layer_id_;
  const void* tile_id_;
  int source_frame_number_;
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
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnWorkerThread");
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_);
    base::TimeTicks start_time = rendering_stats_->StartRecording();
    pixel_ref_->Decode();
    base::TimeDelta duration = rendering_stats_->EndRecording(start_time);
    rendering_stats_->AddDeferredImageDecode(duration);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
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

class RasterFinishedWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(const internal::WorkerPoolTask* source)>
      Callback;

  RasterFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback)
      : origin_loop_(base::MessageLoopProxy::current().get()),
        on_raster_finished_callback_(on_raster_finished_callback) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread");
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RasterFinishedWorkerPoolTaskImpl::RunOnOriginThread,
                   this));
  }
  virtual void CompleteOnOriginThread() OVERRIDE {}

 private:
  virtual ~RasterFinishedWorkerPoolTaskImpl() {}

  void RunOnOriginThread() const {
    on_raster_finished_callback_.Run(this);
  }

  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  const Callback on_raster_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(RasterFinishedWorkerPoolTaskImpl);
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

}  // namespace

namespace internal {

RasterWorkerPoolTask::RasterWorkerPoolTask(
    const Resource* resource, TaskVector* dependencies)
    : did_run_(false),
      did_complete_(false),
      was_canceled_(false),
      resource_(resource) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {
}

void RasterWorkerPoolTask::DidRun(bool was_canceled) {
  DCHECK(!did_run_);
  did_run_ = true;
  was_canceled_ = was_canceled;
}

bool RasterWorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

bool RasterWorkerPoolTask::WasCanceled() const {
  return was_canceled_;
}

void RasterWorkerPoolTask::WillComplete() {
  DCHECK(!did_complete_);
}

void RasterWorkerPoolTask::DidComplete() {
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool RasterWorkerPoolTask::HasCompleted() const {
  return did_complete_;
}

}  // namespace internal

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

// static
RasterWorkerPool::RasterTask RasterWorkerPool::CreateRasterTask(
    const Resource* resource,
    PicturePileImpl* picture_pile,
    gfx::Rect content_rect,
    float contents_scale,
    RasterMode raster_mode,
    bool is_tile_in_pending_tree_now_bin,
    TileResolution tile_resolution,
    int layer_id,
    const void* tile_id,
    int source_frame_number,
    RenderingStatsInstrumentation* rendering_stats,
    const RasterTask::Reply& reply,
    Task::Set* dependencies) {
  return RasterTask(
      new RasterWorkerPoolTaskImpl(resource,
                                   picture_pile,
                                   content_rect,
                                   contents_scale,
                                   raster_mode,
                                   is_tile_in_pending_tree_now_bin,
                                   tile_resolution,
                                   layer_id,
                                   tile_id,
                                   source_frame_number,
                                   rendering_stats,
                                   reply,
                                   &dependencies->tasks_));
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
      resource_provider_(resource_provider),
      weak_ptr_factory_(this) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void RasterWorkerPool::Shutdown() {
  raster_tasks_.clear();
  TaskGraph empty;
  SetTaskGraph(&empty);
  WorkerPool::Shutdown();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterWorkerPool::SetRasterTasks(RasterTask::Queue* queue) {
  raster_tasks_.swap(queue->tasks_);
  raster_tasks_required_for_activation_.swap(
      queue->tasks_required_for_activation_);
}

bool RasterWorkerPool::IsRasterTaskRequiredForActivation(
    internal::RasterWorkerPoolTask* task) const {
  return
      raster_tasks_required_for_activation_.find(task) !=
      raster_tasks_required_for_activation_.end();
}

scoped_refptr<internal::WorkerPoolTask>
    RasterWorkerPool::CreateRasterFinishedTask() {
  return make_scoped_refptr(
      new RasterFinishedWorkerPoolTaskImpl(
          base::Bind(&RasterWorkerPool::OnRasterFinished,
                     weak_ptr_factory_.GetWeakPtr())));
}

scoped_refptr<internal::WorkerPoolTask>
    RasterWorkerPool::CreateRasterRequiredForActivationFinishedTask() {
  return make_scoped_refptr(
      new RasterFinishedWorkerPoolTaskImpl(
          base::Bind(&RasterWorkerPool::OnRasterRequiredForActivationFinished,
                     weak_ptr_factory_.GetWeakPtr())));
}

void RasterWorkerPool::OnRasterFinished(
    const internal::WorkerPoolTask* source) {
  TRACE_EVENT0("cc", "RasterWorkerPool::OnRasterFinished");

  // Early out if current |raster_finished_task_| is not the source.
  if (source != raster_finished_task_.get())
    return;

  OnRasterTasksFinished();
}

void RasterWorkerPool::OnRasterRequiredForActivationFinished(
    const internal::WorkerPoolTask* source) {
  TRACE_EVENT0("cc", "RasterWorkerPool::OnRasterRequiredForActivationFinished");

  // Early out if current |raster_required_for_activation_finished_task_|
  // is not the source.
  if (source != raster_required_for_activation_finished_task_.get())
    return;

  OnRasterTasksRequiredForActivationFinished();
}

scoped_ptr<base::Value> RasterWorkerPool::ScheduledStateAsValue() const {
  scoped_ptr<base::DictionaryValue> scheduled_state(new base::DictionaryValue);
  scheduled_state->SetInteger("task_count", raster_tasks_.size());
  scheduled_state->SetInteger("task_required_for_activation_count",
                              raster_tasks_required_for_activation_.size());
  return scheduled_state.PassAs<base::Value>();
}

// static
internal::GraphNode* RasterWorkerPool::CreateGraphNodeForTask(
    internal::WorkerPoolTask* task,
    unsigned priority,
    TaskGraph* graph) {
  internal::GraphNode* node = new internal::GraphNode(task, priority);
  DCHECK(graph->find(task) == graph->end());
  graph->set(task, make_scoped_ptr(node));
  return node;
}

// static
internal::GraphNode* RasterWorkerPool::CreateGraphNodeForRasterTask(
    internal::WorkerPoolTask* raster_task,
    const TaskVector& decode_tasks,
    unsigned priority,
    TaskGraph* graph) {
  DCHECK(!raster_task->HasCompleted());

  internal::GraphNode* raster_node = CreateGraphNodeForTask(
      raster_task, priority, graph);

  // Insert image decode tasks.
  for (TaskVector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end(); ++it) {
    internal::WorkerPoolTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    raster_node->add_dependency();

    // Check if decode task already exists in graph.
    GraphNodeMap::iterator decode_it = graph->find(decode_task);
    if (decode_it != graph->end()) {
      internal::GraphNode* decode_node = decode_it->second;
      decode_node->add_dependent(raster_node);
      continue;
    }

    internal::GraphNode* decode_node = CreateGraphNodeForTask(
        decode_task, priority, graph);
    decode_node->add_dependent(raster_node);
  }

  return raster_node;
}

}  // namespace cc

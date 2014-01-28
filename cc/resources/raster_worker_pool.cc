// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/debug/trace_event_synthetic_delay.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"

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

// Synthetic delay for raster tasks that are required for activation. Global to
// avoid static initializer on critical path.
struct RasterRequiredForActivationSyntheticDelayInitializer {
  RasterRequiredForActivationSyntheticDelayInitializer()
      : delay(base::debug::TraceEventSyntheticDelay::Lookup(
            "cc.RasterRequiredForActivation")) {}
  base::debug::TraceEventSyntheticDelay* delay;
};
static base::LazyInstance<RasterRequiredForActivationSyntheticDelayInitializer>
    g_raster_required_for_activation_delay = LAZY_INSTANCE_INITIALIZER;

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
                           const gfx::Rect& content_rect,
                           float contents_scale,
                           RasterMode raster_mode,
                           TileResolution tile_resolution,
                           int layer_id,
                           const void* tile_id,
                           int source_frame_number,
                           bool use_gpu_rasterization,
                           RenderingStatsInstrumentation* rendering_stats,
                           const RasterWorkerPool::RasterTask::Reply& reply,
                           internal::Task::Vector* dependencies)
      : internal::RasterWorkerPoolTask(resource,
                                       dependencies,
                                       use_gpu_rasterization),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
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

    picture_clone->AnalyzeInRect(
        content_rect_, contents_scale_, &analysis_, rendering_stats_);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  bool RunRasterOnThread(unsigned thread_index,
                         void* buffer,
                         const gfx::Size& size,
                         int stride) {
    TRACE_EVENT2(
        "cc",
        "RasterWorkerPoolTaskImpl::RunRasterOnThread",
        "data",
        TracedValue::FromValue(DataAsValue().release()),
        "raster_mode",
        TracedValue::FromValue(RasterModeAsValue(raster_mode_).release()));

    devtools_instrumentation::ScopedLayerTask raster_task(
        devtools_instrumentation::kRasterTask, layer_id_);

    DCHECK(picture_pile_.get());
    DCHECK(buffer);

    if (analysis_.is_solid_color)
      return false;

    SkBitmap bitmap;
    switch (resource()->format()) {
      case RGBA_4444:
        // Use the default stride if we will eventually convert this
        // bitmap to 4444.
        bitmap.setConfig(
            SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        break;
      case RGBA_8888:
      case BGRA_8888:
        bitmap.setConfig(
            SkBitmap::kARGB_8888_Config, size.width(), size.height(), stride);
        bitmap.setPixels(buffer);
        break;
      case LUMINANCE_8:
      case RGB_565:
      case ETC1:
        NOTREACHED();
        break;
    }

    SkBitmapDevice device(bitmap);
    SkCanvas canvas(&device);
    Raster(picture_pile_->GetCloneForDrawingOnThread(thread_index), &canvas);
    ChangeBitmapConfigIfNeeded(bitmap, buffer);

    return true;
  }

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnWorkerThread(unsigned thread_index,
                                 void* buffer,
                                 const gfx::Size& size,
                                 int stride) OVERRIDE {
    // TODO(alokp): For now run-on-worker-thread implies software rasterization.
    DCHECK(!use_gpu_rasterization());
    RunAnalysisOnThread(thread_index);
    return RunRasterOnThread(thread_index, buffer, size, stride);
  }

  virtual void RunOnOriginThread(ResourceProvider* resource_provider,
                                 ContextProvider* context_provider) OVERRIDE {
    // TODO(alokp): For now run-on-origin-thread implies gpu rasterization.
    DCHECK(use_gpu_rasterization());
    ResourceProvider::ScopedWriteLockGL lock(resource_provider,
                                             resource()->id());
    DCHECK_NE(lock.texture_id(), 0u);

    GrBackendTextureDesc desc;
    desc.fFlags = kRenderTarget_GrBackendTextureFlag;
    desc.fWidth = content_rect_.width();
    desc.fHeight = content_rect_.height();
    desc.fConfig = ToGrFormat(resource()->format());
    desc.fOrigin = kTopLeft_GrSurfaceOrigin;
    desc.fTextureHandle = lock.texture_id();

    GrContext* gr_context = context_provider->GrContext();
    skia::RefPtr<GrTexture> texture =
        skia::AdoptRef(gr_context->wrapBackendTexture(desc));
    skia::RefPtr<SkGpuDevice> device =
        skia::AdoptRef(SkGpuDevice::Create(texture.get()));
    skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(new SkCanvas(device.get()));

    Raster(picture_pile_, canvas.get());
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
    res->Set("resolution", TileResolutionAsValue(tile_resolution_).release());
    res->SetInteger("source_frame_number", source_frame_number_);
    res->SetInteger("layer_id", layer_id_);
    return res.PassAs<base::Value>();
  }

  static GrPixelConfig ToGrFormat(ResourceFormat format) {
    switch (format) {
      case RGBA_8888:
        return kRGBA_8888_GrPixelConfig;
      case BGRA_8888:
        return kBGRA_8888_GrPixelConfig;
      case RGBA_4444:
        return kRGBA_4444_GrPixelConfig;
      default:
        break;
    }
    DCHECK(false) << "Unsupported resource format.";
    return kSkia8888_GrPixelConfig;
  }

  void Raster(PicturePileImpl* picture_pile, SkCanvas* canvas) {
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
    canvas->setDrawFilter(draw_filter.get());

    base::TimeDelta prev_rasterize_time =
        rendering_stats_->impl_thread_rendering_stats().rasterize_time;

    // Only record rasterization time for highres tiles, because
    // lowres tiles are not required for activation and therefore
    // introduce noise in the measurement (sometimes they get rasterized
    // before we draw and sometimes they aren't)
    RenderingStatsInstrumentation* stats =
        tile_resolution_ == HIGH_RESOLUTION ? rendering_stats_ : NULL;
    picture_pile->RasterToBitmap(canvas, content_rect_, contents_scale_, stats);

    if (rendering_stats_->record_rendering_stats()) {
      base::TimeDelta current_rasterize_time =
          rendering_stats_->impl_thread_rendering_stats().rasterize_time;
      HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.PictureRasterTimeUS",
          (current_rasterize_time - prev_rasterize_time).InMicroseconds(),
          0,
          100000,
          100);
    }
  }

  void ChangeBitmapConfigIfNeeded(const SkBitmap& bitmap, void* buffer) {
    TRACE_EVENT0("cc", "RasterWorkerPoolTaskImpl::ChangeBitmapConfigIfNeeded");
    SkBitmap::Config config = SkBitmapConfig(resource()->format());
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
  ImageDecodeWorkerPoolTaskImpl(SkPixelRef* pixel_ref,
                                int layer_id,
                                RenderingStatsInstrumentation* rendering_stats,
                                const RasterWorkerPool::Task::Reply& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnWorkerThread");
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  virtual ~ImageDecodeWorkerPoolTaskImpl() {}

 private:
  skia::RefPtr<SkPixelRef> pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const RasterWorkerPool::Task::Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeWorkerPoolTaskImpl);
};

class RasterFinishedWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(const internal::WorkerPoolTask* source)> Callback;

  explicit RasterFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback)
      : origin_loop_(base::MessageLoopProxy::current().get()),
        on_raster_finished_callback_(on_raster_finished_callback) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread");
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RasterFinishedWorkerPoolTaskImpl::RunOnOriginThread, this));
  }
  virtual void CompleteOnOriginThread() OVERRIDE {}

 protected:
  virtual ~RasterFinishedWorkerPoolTaskImpl() {}

 private:
  void RunOnOriginThread() const { on_raster_finished_callback_.Run(this); }

  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  const Callback on_raster_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(RasterFinishedWorkerPoolTaskImpl);
};

class RasterRequiredForActivationFinishedWorkerPoolTaskImpl
    : public RasterFinishedWorkerPoolTaskImpl {
 public:
  RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback,
      size_t tasks_required_for_activation_count)
      : RasterFinishedWorkerPoolTaskImpl(on_raster_finished_callback),
        tasks_required_for_activation_count_(
            tasks_required_for_activation_count) {
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->BeginParallel(
          &activation_delay_end_time_);
    }
  }

  // Overridden from RasterFinishedWorkerPoolTaskImpl:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc",
                 "RasterRequiredForActivationFinishedWorkerPoolTaskImpl::"
                 "RunOnWorkerThread");
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->EndParallel(
          activation_delay_end_time_);
    }
    RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread(thread_index);
  }

 private:
  virtual ~RasterRequiredForActivationFinishedWorkerPoolTaskImpl() {}

  base::TimeTicks activation_delay_end_time_;
  const size_t tasks_required_for_activation_count_;

  DISALLOW_COPY_AND_ASSIGN(
      RasterRequiredForActivationFinishedWorkerPoolTaskImpl);
};

class RasterTaskGraphRunner : public internal::TaskGraphRunner {
 public:
  RasterTaskGraphRunner()
      : internal::TaskGraphRunner(RasterWorkerPool::GetNumRasterThreads(),
                                  "CompositorRaster") {}
};
base::LazyInstance<RasterTaskGraphRunner>::Leaky g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumRasterThreads = 1;

int g_num_raster_threads = 0;

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask() : did_complete_(false) {}

WorkerPoolTask::~WorkerPoolTask() {
  DCHECK_EQ(did_schedule_, did_complete_);
  DCHECK(!did_run_ || did_complete_);
}

void WorkerPoolTask::WillComplete() { DCHECK(!did_complete_); }

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool WorkerPoolTask::HasCompleted() const { return did_complete_; }

RasterWorkerPoolTask::RasterWorkerPoolTask(const Resource* resource,
                                           internal::Task::Vector* dependencies,
                                           bool use_gpu_rasterization)
    : did_run_(false),
      did_complete_(false),
      was_canceled_(false),
      resource_(resource),
      use_gpu_rasterization_(use_gpu_rasterization) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {}

void RasterWorkerPoolTask::DidRun(bool was_canceled) {
  DCHECK(!did_run_);
  did_run_ = true;
  was_canceled_ = was_canceled;
}

bool RasterWorkerPoolTask::HasFinishedRunning() const { return did_run_; }

bool RasterWorkerPoolTask::WasCanceled() const { return was_canceled_; }

void RasterWorkerPoolTask::WillComplete() { DCHECK(!did_complete_); }

void RasterWorkerPoolTask::DidComplete() {
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool RasterWorkerPoolTask::HasCompleted() const { return did_complete_; }

}  // namespace internal

RasterWorkerPool::Task::Set::Set() {}

RasterWorkerPool::Task::Set::~Set() {}

void RasterWorkerPool::Task::Set::Insert(const Task& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::Task::Task() {}

RasterWorkerPool::Task::Task(internal::WorkerPoolTask* internal)
    : internal_(internal) {}

RasterWorkerPool::Task::~Task() {}

void RasterWorkerPool::Task::Reset() { internal_ = NULL; }

RasterWorkerPool::RasterTask::Queue::Queue() {}

RasterWorkerPool::RasterTask::Queue::~Queue() {}

void RasterWorkerPool::RasterTask::Queue::Append(const RasterTask& task,
                                                 bool required_for_activation) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
  if (required_for_activation)
    tasks_required_for_activation_.insert(task.internal_.get());
}

RasterWorkerPool::RasterTask::RasterTask() {}

RasterWorkerPool::RasterTask::RasterTask(
    internal::RasterWorkerPoolTask* internal)
    : internal_(internal) {}

void RasterWorkerPool::RasterTask::Reset() { internal_ = NULL; }

RasterWorkerPool::RasterTask::~RasterTask() {}

RasterWorkerPool::RasterWorkerPool(ResourceProvider* resource_provider,
                                   ContextProvider* context_provider)
    : namespace_token_(g_task_graph_runner.Pointer()->GetNamespaceToken()),
      client_(NULL),
      resource_provider_(resource_provider),
      context_provider_(context_provider),
      weak_ptr_factory_(this) {}

RasterWorkerPool::~RasterWorkerPool() {}

// static
void RasterWorkerPool::SetNumRasterThreads(int num_threads) {
  DCHECK_LT(0, num_threads);
  DCHECK_EQ(0, g_num_raster_threads);

  g_num_raster_threads = num_threads;
}

// static
int RasterWorkerPool::GetNumRasterThreads() {
  if (!g_num_raster_threads)
    g_num_raster_threads = kDefaultNumRasterThreads;

  return g_num_raster_threads;
}

// static
RasterWorkerPool::RasterTask RasterWorkerPool::CreateRasterTask(
    const Resource* resource,
    PicturePileImpl* picture_pile,
    const gfx::Rect& content_rect,
    float contents_scale,
    RasterMode raster_mode,
    TileResolution tile_resolution,
    int layer_id,
    const void* tile_id,
    int source_frame_number,
    bool use_gpu_rasterization,
    RenderingStatsInstrumentation* rendering_stats,
    const RasterTask::Reply& reply,
    Task::Set* dependencies) {
  return RasterTask(new RasterWorkerPoolTaskImpl(resource,
                                                 picture_pile,
                                                 content_rect,
                                                 contents_scale,
                                                 raster_mode,
                                                 tile_resolution,
                                                 layer_id,
                                                 tile_id,
                                                 source_frame_number,
                                                 use_gpu_rasterization,
                                                 rendering_stats,
                                                 reply,
                                                 &dependencies->tasks_));
}

// static
RasterWorkerPool::Task RasterWorkerPool::CreateImageDecodeTask(
    SkPixelRef* pixel_ref,
    int layer_id,
    RenderingStatsInstrumentation* stats_instrumentation,
    const Task::Reply& reply) {
  return Task(new ImageDecodeWorkerPoolTaskImpl(
      pixel_ref, layer_id, stats_instrumentation, reply));
}

void RasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void RasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "RasterWorkerPool::Shutdown");

  raster_tasks_.clear();
  TaskGraph empty;
  SetTaskGraph(&empty);
  g_task_graph_runner.Pointer()->WaitForTasksToFinishRunning(namespace_token_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "RasterWorkerPool::CheckForCompletedTasks");

  CheckForCompletedWorkerPoolTasks();

  // Complete gpu rasterization tasks.
  while (!completed_gpu_raster_tasks_.empty()) {
    internal::RasterWorkerPoolTask* task =
        completed_gpu_raster_tasks_.front().get();

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();

    completed_gpu_raster_tasks_.pop_front();
  }
}

void RasterWorkerPool::CheckForCompletedWorkerPoolTasks() {
  internal::Task::Vector completed_tasks;
  g_task_graph_runner.Pointer()->CollectCompletedTasks(namespace_token_,
                                                       &completed_tasks);

  for (internal::Task::Vector::const_iterator it = completed_tasks.begin();
       it != completed_tasks.end();
       ++it) {
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();
  }
}

void RasterWorkerPool::SetTaskGraph(TaskGraph* graph) {
  TRACE_EVENT1(
      "cc", "RasterWorkerPool::SetTaskGraph", "num_tasks", graph->size());

  g_task_graph_runner.Pointer()->SetTaskGraph(namespace_token_, graph);
}

void RasterWorkerPool::SetRasterTasks(RasterTask::Queue* queue) {
  raster_tasks_.swap(queue->tasks_);
  raster_tasks_required_for_activation_.swap(
      queue->tasks_required_for_activation_);
}

bool RasterWorkerPool::IsRasterTaskRequiredForActivation(
    internal::RasterWorkerPoolTask* task) const {
  return raster_tasks_required_for_activation_.find(task) !=
         raster_tasks_required_for_activation_.end();
}

void RasterWorkerPool::RunGpuRasterTasks(const RasterTaskVector& tasks) {
  if (tasks.empty())
    return;

  GrContext* gr_context = context_provider_->GrContext();
  // TODO(alokp): Implement TestContextProvider::GrContext().
  if (gr_context)
    gr_context->resetContext();

  for (RasterTaskVector::const_iterator it = tasks.begin(); it != tasks.end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(task->use_gpu_rasterization());

    task->RunOnOriginThread(resource_provider_, context_provider_);
    task->DidRun(false);
    completed_gpu_raster_tasks_.push_back(task);
  }

  // TODO(alokp): Implement TestContextProvider::GrContext().
  if (gr_context)
    gr_context->flush();
}

scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterFinishedTask() {
  return make_scoped_refptr(new RasterFinishedWorkerPoolTaskImpl(base::Bind(
      &RasterWorkerPool::OnRasterFinished, weak_ptr_factory_.GetWeakPtr())));
}

scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterRequiredForActivationFinishedTask(
    size_t tasks_required_for_activation_count) {
  return make_scoped_refptr(
      new RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
          base::Bind(&RasterWorkerPool::OnRasterRequiredForActivationFinished,
                     weak_ptr_factory_.GetWeakPtr()),
          tasks_required_for_activation_count));
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
    const internal::Task::Vector& decode_tasks,
    unsigned priority,
    TaskGraph* graph) {
  DCHECK(!raster_task->HasCompleted());

  internal::GraphNode* raster_node =
      CreateGraphNodeForTask(raster_task, priority, graph);

  // Insert image decode tasks.
  for (internal::Task::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end();
       ++it) {
    internal::WorkerPoolTask* decode_task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    raster_node->add_dependency();

    // Check if decode task already exists in graph.
    internal::GraphNode::Map::iterator decode_it = graph->find(decode_task);
    if (decode_it != graph->end()) {
      internal::GraphNode* decode_node = decode_it->second;
      decode_node->add_dependent(raster_node);
      continue;
    }

    internal::GraphNode* decode_node =
        CreateGraphNodeForTask(decode_task, priority, graph);
    decode_node->add_dependent(raster_node);
  }

  return raster_node;
}

}  // namespace cc

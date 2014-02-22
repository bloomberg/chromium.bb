// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event_synthetic_delay.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrContext.h"

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
  RasterWorkerPoolTaskImpl(
      const Resource* resource,
      PicturePileImpl* picture_pile,
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterMode raster_mode,
      TileResolution tile_resolution,
      int layer_id,
      const void* tile_id,
      int source_frame_number,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(const PicturePileImpl::Analysis&, bool)>& reply,
      internal::WorkerPoolTask::Vector* dependencies,
      ContextProvider* context_provider)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        tile_id_(tile_id),
        source_frame_number_(source_frame_number),
        rendering_stats_(rendering_stats),
        reply_(reply),
        context_provider_(context_provider),
        canvas_(NULL) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "RasterWorkerPoolTaskImpl::RunOnWorkerThread");

    DCHECK(picture_pile_);
    Analyze(picture_pile_->GetCloneForDrawingOnThread(thread_index));
    if (!canvas_ || analysis_.is_solid_color)
      return;
    Raster(picture_pile_->GetCloneForDrawingOnThread(thread_index));
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    DCHECK(!canvas_);
    canvas_ = client->AcquireCanvasForRaster(this, resource());
  }
  virtual void RunOnOriginThread() OVERRIDE {
    TRACE_EVENT0("cc", "RasterWorkerPoolTaskImpl::RunOnOriginThread");

    Analyze(picture_pile_);
    if (!canvas_ || analysis_.is_solid_color)
      return;
    // TODO(alokp): Use a trace macro to push/pop markers.
    // Using push/pop functions directly incurs cost to evaluate function
    // arguments even when tracing is disabled.
    DCHECK(context_provider_);
    context_provider_->ContextGL()->PushGroupMarkerEXT(
        0,
        base::StringPrintf(
            "Raster-%d-%d-%p", source_frame_number_, layer_id_, tile_id_)
            .c_str());
    Raster(picture_pile_);
    context_provider_->ContextGL()->PopGroupMarkerEXT();
  }
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    canvas_ = NULL;
    client->ReleaseCanvasForRaster(this, resource());
  }
  virtual void RunReplyOnOriginThread() OVERRIDE {
    DCHECK(!canvas_);
    reply_.Run(analysis_, !HasFinishedRunning());
  }

 protected:
  virtual ~RasterWorkerPoolTaskImpl() { DCHECK(!canvas_); }

 private:
  scoped_ptr<base::Value> DataAsValue() const {
    scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
    res->Set("tile_id", TracedValue::CreateIDRef(tile_id_).release());
    res->Set("resolution", TileResolutionAsValue(tile_resolution_).release());
    res->SetInteger("source_frame_number", source_frame_number_);
    res->SetInteger("layer_id", layer_id_);
    return res.PassAs<base::Value>();
  }

  void Analyze(PicturePileImpl* picture_pile) {
    TRACE_EVENT1("cc",
                 "RasterWorkerPoolTaskImpl::Analyze",
                 "data",
                 TracedValue::FromValue(DataAsValue().release()));

    DCHECK(picture_pile);

    picture_pile->AnalyzeInRect(
        content_rect_, contents_scale_, &analysis_, rendering_stats_);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  void Raster(PicturePileImpl* picture_pile) {
    TRACE_EVENT2(
        "cc",
        "RasterWorkerPoolTaskImpl::Raster",
        "data",
        TracedValue::FromValue(DataAsValue().release()),
        "raster_mode",
        TracedValue::FromValue(RasterModeAsValue(raster_mode_).release()));

    devtools_instrumentation::ScopedLayerTask raster_task(
        devtools_instrumentation::kRasterTask, layer_id_);

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
    canvas_->setDrawFilter(draw_filter.get());

    base::TimeDelta prev_rasterize_time =
        rendering_stats_->impl_thread_rendering_stats().rasterize_time;

    // Only record rasterization time for highres tiles, because
    // lowres tiles are not required for activation and therefore
    // introduce noise in the measurement (sometimes they get rasterized
    // before we draw and sometimes they aren't)
    RenderingStatsInstrumentation* stats =
        tile_resolution_ == HIGH_RESOLUTION ? rendering_stats_ : NULL;
    DCHECK(picture_pile);
    picture_pile->RasterToBitmap(
        canvas_, content_rect_, contents_scale_, stats);

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
  const base::Callback<void(const PicturePileImpl::Analysis&, bool)> reply_;
  ContextProvider* context_provider_;
  SkCanvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPoolTaskImpl);
};

class ImageDecodeWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  ImageDecodeWorkerPoolTaskImpl(
      SkPixelRef* pixel_ref,
      int layer_id,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(bool was_canceled)>& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnWorkerThread");
    Decode();
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunOnOriginThread() OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnOriginThread");
    Decode();
  }
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  virtual ~ImageDecodeWorkerPoolTaskImpl() {}

 private:
  void Decode() {
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();
  }

  skia::RefPtr<SkPixelRef> pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const base::Callback<void(bool was_canceled)>& reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeWorkerPoolTaskImpl);
};

class RasterFinishedWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(const internal::WorkerPoolTask* source)> Callback;

  explicit RasterFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback)
      : origin_loop_(base::MessageLoopProxy::current().get()),
        on_raster_finished_callback_(on_raster_finished_callback) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread");
    RasterFinished();
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunOnOriginThread() OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedWorkerPoolTaskImpl::RunOnOriginThread");
    RasterFinished();
  }
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE {}

 protected:
  virtual ~RasterFinishedWorkerPoolTaskImpl() {}

  void RasterFinished() {
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &RasterFinishedWorkerPoolTaskImpl::OnRasterFinishedOnOriginThread,
            this));
  }

 private:
  void OnRasterFinishedOnOriginThread() const {
    on_raster_finished_callback_.Run(this);
  }

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

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc",
                 "RasterRequiredForActivationFinishedWorkerPoolTaskImpl::"
                 "RunOnWorkerThread");
    RunRasterFinished();
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnOriginThread() OVERRIDE {
    TRACE_EVENT0("cc",
                 "RasterRequiredForActivationFinishedWorkerPoolTaskImpl::"
                 "RunOnOriginThread");
    RunRasterFinished();
  }

 private:
  virtual ~RasterRequiredForActivationFinishedWorkerPoolTaskImpl() {}

  void RunRasterFinished() {
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->EndParallel(
          activation_delay_end_time_);
    }
    RasterFinished();
  }

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

WorkerPoolTask::WorkerPoolTask() : did_schedule_(false), did_complete_(false) {}

WorkerPoolTask::~WorkerPoolTask() {
  DCHECK(!did_schedule_);
  DCHECK(!did_run_ || did_complete_);
}

void WorkerPoolTask::WillSchedule() { DCHECK(!did_schedule_); }

void WorkerPoolTask::DidSchedule() {
  did_schedule_ = true;
  did_complete_ = false;
}

bool WorkerPoolTask::HasBeenScheduled() const { return did_schedule_; }

void WorkerPoolTask::WillComplete() { DCHECK(!did_complete_); }

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_schedule_ = false;
  did_complete_ = true;
}

bool WorkerPoolTask::HasCompleted() const { return did_complete_; }

RasterWorkerPoolTask::RasterWorkerPoolTask(
    const Resource* resource,
    internal::WorkerPoolTask::Vector* dependencies)
    : resource_(resource) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {}

}  // namespace internal

RasterTaskQueue::Item::Item(internal::RasterWorkerPoolTask* task,
                            bool required_for_activation)
    : task(task), required_for_activation(required_for_activation) {}

RasterTaskQueue::Item::~Item() {}

RasterTaskQueue::RasterTaskQueue() : required_for_activation_count(0u) {}

RasterTaskQueue::~RasterTaskQueue() {}

void RasterTaskQueue::Swap(RasterTaskQueue* other) {
  items.swap(other->items);
  std::swap(required_for_activation_count,
            other->required_for_activation_count);
}

void RasterTaskQueue::Reset() {
  required_for_activation_count = 0u;
  items.clear();
}

// This allows an external rasterize on-demand system to run raster tasks
// with highest priority using the same task graph runner instance.
unsigned RasterWorkerPool::kOnDemandRasterTaskPriority = 0u;
// Task priorities that make sure raster finished tasks run before any
// remaining raster tasks.
unsigned RasterWorkerPool::kRasterFinishedTaskPriority = 2u;
unsigned RasterWorkerPool::kRasterRequiredForActivationFinishedTaskPriority =
    1u;
unsigned RasterWorkerPool::kRasterTaskPriorityBase = 3u;

RasterWorkerPool::RasterWorkerPool(internal::TaskGraphRunner* task_graph_runner,
                                   ResourceProvider* resource_provider)
    : task_graph_runner_(task_graph_runner),
      client_(NULL),
      resource_provider_(resource_provider),
      weak_ptr_factory_(this) {
  if (task_graph_runner_)
    namespace_token_ = task_graph_runner_->GetNamespaceToken();
}

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
internal::TaskGraphRunner* RasterWorkerPool::GetTaskGraphRunner() {
  return g_task_graph_runner.Pointer();
}

// static
scoped_refptr<internal::RasterWorkerPoolTask>
RasterWorkerPool::CreateRasterTask(
    const Resource* resource,
    PicturePileImpl* picture_pile,
    const gfx::Rect& content_rect,
    float contents_scale,
    RasterMode raster_mode,
    TileResolution tile_resolution,
    int layer_id,
    const void* tile_id,
    int source_frame_number,
    RenderingStatsInstrumentation* rendering_stats,
    const base::Callback<void(const PicturePileImpl::Analysis&, bool)>& reply,
    internal::WorkerPoolTask::Vector* dependencies,
    ContextProvider* context_provider) {
  return make_scoped_refptr(new RasterWorkerPoolTaskImpl(resource,
                                                         picture_pile,
                                                         content_rect,
                                                         contents_scale,
                                                         raster_mode,
                                                         tile_resolution,
                                                         layer_id,
                                                         tile_id,
                                                         source_frame_number,
                                                         rendering_stats,
                                                         reply,
                                                         dependencies,
                                                         context_provider));
}

// static
scoped_refptr<internal::WorkerPoolTask> RasterWorkerPool::CreateImageDecodeTask(
    SkPixelRef* pixel_ref,
    int layer_id,
    RenderingStatsInstrumentation* rendering_stats,
    const base::Callback<void(bool was_canceled)>& reply) {
  return make_scoped_refptr(new ImageDecodeWorkerPoolTaskImpl(
      pixel_ref, layer_id, rendering_stats, reply));
}

void RasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void RasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "RasterWorkerPool::Shutdown");

  if (task_graph_runner_) {
    internal::TaskGraph empty;
    task_graph_runner_->SetTaskGraph(namespace_token_, &empty);
    task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterWorkerPool::SetTaskGraph(internal::TaskGraph* graph) {
  TRACE_EVENT0("cc", "RasterWorkerPool::SetTaskGraph");

  DCHECK(task_graph_runner_);
  for (internal::TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end();
       ++it) {
    internal::TaskGraph::Node& node = *it;
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();
    }
  }

  task_graph_runner_->SetTaskGraph(namespace_token_, graph);
}

void RasterWorkerPool::CollectCompletedWorkerPoolTasks(
    internal::Task::Vector* completed_tasks) {
  DCHECK(task_graph_runner_);
  task_graph_runner_->CollectCompletedTasks(namespace_token_, completed_tasks);
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

void RasterWorkerPool::RunTaskOnOriginThread(internal::WorkerPoolTask* task) {
  task->WillSchedule();
  task->ScheduleOnOriginThread(this);
  task->DidSchedule();

  task->WillRun();
  task->RunOnOriginThread();
  task->DidRun();

  task->WillComplete();
  task->CompleteOnOriginThread(this);
  task->DidComplete();
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

// static
void RasterWorkerPool::InsertNodeForTask(internal::TaskGraph* graph,
                                         internal::WorkerPoolTask* task,
                                         unsigned priority,
                                         size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(),
                      graph->nodes.end(),
                      internal::TaskGraph::Node::TaskComparator(task)) ==
         graph->nodes.end());
  graph->nodes.push_back(
      internal::TaskGraph::Node(task, priority, dependencies));
}

// static
void RasterWorkerPool::InsertNodeForRasterTask(
    internal::TaskGraph* graph,
    internal::WorkerPoolTask* raster_task,
    const internal::WorkerPoolTask::Vector& decode_tasks,
    unsigned priority) {
  size_t dependencies = 0u;

  // Insert image decode tasks.
  for (internal::WorkerPoolTask::Vector::const_iterator it =
           decode_tasks.begin();
       it != decode_tasks.end();
       ++it) {
    internal::WorkerPoolTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exists in graph.
    internal::TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(),
                     graph->nodes.end(),
                     internal::TaskGraph::Node::TaskComparator(decode_task));
    if (decode_it == graph->nodes.end())
      InsertNodeForTask(graph, decode_task, priority, 0u);

    graph->edges.push_back(internal::TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(graph, raster_task, priority, dependencies);
}

}  // namespace cc

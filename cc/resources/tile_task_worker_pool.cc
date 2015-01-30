// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_task_worker_pool.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/resources/raster_source.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cc {
namespace {

base::ThreadPriority g_worker_thread_priority = base::kThreadPriority_Normal;

class TileTaskGraphRunner : public TaskGraphRunner,
                            public base::DelegateSimpleThread::Delegate {
 public:
  TileTaskGraphRunner() {
    size_t num_threads = TileTaskWorkerPool::GetNumWorkerThreads();
    while (workers_.size() < num_threads) {
      scoped_ptr<base::DelegateSimpleThread> worker =
          make_scoped_ptr(new base::DelegateSimpleThread(
              this, base::StringPrintf(
                        "CompositorTileWorker%u",
                        static_cast<unsigned>(workers_.size() + 1)).c_str()));
      worker->Start();
      worker->SetThreadPriority(g_worker_thread_priority);
      workers_.push_back(worker.Pass());
    }
  }

  ~TileTaskGraphRunner() override { NOTREACHED(); }

 private:
  // Overridden from base::DelegateSimpleThread::Delegate:
  void Run() override { TaskGraphRunner::Run(); }

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;
};

base::LazyInstance<TileTaskGraphRunner>::Leaky g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumWorkerThreads = 1;

int g_num_worker_threads = 0;

class TaskSetFinishedTaskImpl : public TileTask {
 public:
  explicit TaskSetFinishedTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_task_set_finished_callback)
      : task_runner_(task_runner),
        on_task_set_finished_callback_(on_task_set_finished_callback) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "TaskSetFinishedTaskImpl::RunOnWorkerThread");
    TaskSetFinished();
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {}
  void CompleteOnOriginThread(TileTaskClient* client) override {}
  void RunReplyOnOriginThread() override {}

 protected:
  ~TaskSetFinishedTaskImpl() override {}

  void TaskSetFinished() {
    task_runner_->PostTask(FROM_HERE, on_task_set_finished_callback_);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Closure on_task_set_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskSetFinishedTaskImpl);
};

}  // namespace

// This allows a micro benchmark system to run tasks with highest priority,
// since it should finish as quickly as possible.
unsigned TileTaskWorkerPool::kBenchmarkTaskPriority = 0u;
// Task priorities that make sure task set finished tasks run before any
// other remaining tasks. This is combined with the task set type to ensure
// proper prioritization ordering between task set types.
unsigned TileTaskWorkerPool::kTaskSetFinishedTaskPriorityBase = 1u;
// For correctness, |kTileTaskPriorityBase| must be greater than
// |kTaskSetFinishedTaskPriorityBase + kNumberOfTaskSets|.
unsigned TileTaskWorkerPool::kTileTaskPriorityBase = 10u;

TileTaskWorkerPool::TileTaskWorkerPool() {
}

TileTaskWorkerPool::~TileTaskWorkerPool() {
}

// static
void TileTaskWorkerPool::SetNumWorkerThreads(int num_threads) {
  DCHECK_LT(0, num_threads);
  DCHECK_EQ(0, g_num_worker_threads);

  g_num_worker_threads = num_threads;
}

// static
int TileTaskWorkerPool::GetNumWorkerThreads() {
  if (!g_num_worker_threads)
    g_num_worker_threads = kDefaultNumWorkerThreads;

  return g_num_worker_threads;
}

// static
void TileTaskWorkerPool::SetWorkerThreadPriority(
    base::ThreadPriority priority) {
  g_worker_thread_priority = priority;
}

// static
TaskGraphRunner* TileTaskWorkerPool::GetTaskGraphRunner() {
  return g_task_graph_runner.Pointer();
}

// static
scoped_refptr<TileTask> TileTaskWorkerPool::CreateTaskSetFinishedTask(
    base::SequencedTaskRunner* task_runner,
    const base::Closure& on_task_set_finished_callback) {
  return make_scoped_refptr(
      new TaskSetFinishedTaskImpl(task_runner, on_task_set_finished_callback));
}

// static
void TileTaskWorkerPool::ScheduleTasksOnOriginThread(TileTaskClient* client,
                                                     TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskWorkerPool::ScheduleTasksOnOriginThread");

  for (TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end(); ++it) {
    TaskGraph::Node& node = *it;
    TileTask* task = static_cast<TileTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(client);
      task->DidSchedule();
    }
  }
}

// static
void TileTaskWorkerPool::InsertNodeForTask(TaskGraph* graph,
                                           TileTask* task,
                                           unsigned priority,
                                           size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(), graph->nodes.end(),
                      TaskGraph::Node::TaskComparator(task)) ==
         graph->nodes.end());
  graph->nodes.push_back(TaskGraph::Node(task, priority, dependencies));
}

// static
void TileTaskWorkerPool::InsertNodesForRasterTask(
    TaskGraph* graph,
    RasterTask* raster_task,
    const ImageDecodeTask::Vector& decode_tasks,
    unsigned priority) {
  size_t dependencies = 0u;

  // Insert image decode tasks.
  for (ImageDecodeTask::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end(); ++it) {
    ImageDecodeTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exists in graph.
    TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(), graph->nodes.end(),
                     TaskGraph::Node::TaskComparator(decode_task));
    if (decode_it == graph->nodes.end())
      InsertNodeForTask(graph, decode_task, priority, 0u);

    graph->edges.push_back(TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(graph, raster_task, priority, dependencies);
}

static bool IsSupportedPlaybackToMemoryFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
    case RED_8:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
void TileTaskWorkerPool::PlaybackToMemory(void* memory,
                                          ResourceFormat format,
                                          const gfx::Size& size,
                                          int stride,
                                          const RasterSource* raster_source,
                                          const gfx::Rect& rect,
                                          float scale) {
  DCHECK(IsSupportedPlaybackToMemoryFormat(format)) << format;

  // Uses kPremul_SkAlphaType since the result is not known to be opaque.
  SkImageInfo info =
      SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType);
  SkColorType buffer_color_type = ResourceFormatToSkColorType(format);
  bool needs_copy = buffer_color_type != info.colorType();

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (raster_source->CanUseLCDText()) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props = SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);
  }

  if (!stride)
    stride = info.minRowBytes();

  if (!needs_copy) {
    skia::RefPtr<SkSurface> surface = skia::AdoptRef(
        SkSurface::NewRasterDirect(info, memory, stride, &surface_props));
    skia::RefPtr<SkCanvas> canvas = skia::SharePtr(surface->getCanvas());
    raster_source->PlaybackToCanvas(canvas.get(), rect, scale);
    return;
  }

  skia::RefPtr<SkSurface> surface =
      skia::AdoptRef(SkSurface::NewRaster(info, &surface_props));
  skia::RefPtr<SkCanvas> canvas = skia::SharePtr(surface->getCanvas());
  raster_source->PlaybackToCanvas(canvas.get(), rect, scale);

  SkImageInfo dst_info = info;
  dst_info.fColorType = buffer_color_type;
  // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
  // bitmap data. There will be no need to call SkAlign4 once crbug.com/293728
  // is fixed.
  const size_t dst_row_bytes = SkAlign4(dst_info.minRowBytes());
  DCHECK_EQ(0u, dst_row_bytes % 4);
  bool success = canvas->readPixels(dst_info, memory, dst_row_bytes, 0, 0);
  DCHECK_EQ(true, success);
}

}  // namespace cc

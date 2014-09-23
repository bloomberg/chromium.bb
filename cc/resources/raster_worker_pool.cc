// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {
namespace {

class RasterTaskGraphRunner : public TaskGraphRunner,
                              public base::DelegateSimpleThread::Delegate {
 public:
  RasterTaskGraphRunner() {
    size_t num_threads = RasterWorkerPool::GetNumRasterThreads();
    while (workers_.size() < num_threads) {
      scoped_ptr<base::DelegateSimpleThread> worker =
          make_scoped_ptr(new base::DelegateSimpleThread(
              this,
              base::StringPrintf("CompositorRasterWorker%u",
                                 static_cast<unsigned>(workers_.size() + 1))
                  .c_str()));
      worker->Start();
#if defined(OS_ANDROID) || defined(OS_LINUX)
      worker->SetThreadPriority(base::kThreadPriority_Background);
#endif
      workers_.push_back(worker.Pass());
    }
  }

  virtual ~RasterTaskGraphRunner() { NOTREACHED(); }

 private:
  // Overridden from base::DelegateSimpleThread::Delegate:
  virtual void Run() OVERRIDE {
    TaskGraphRunner::Run();
  }

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;
};

base::LazyInstance<RasterTaskGraphRunner>::Leaky g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumRasterThreads = 1;

int g_num_raster_threads = 0;

class RasterFinishedTaskImpl : public RasterizerTask {
 public:
  explicit RasterFinishedTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_raster_finished_callback)
      : task_runner_(task_runner),
        on_raster_finished_callback_(on_raster_finished_callback) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedTaskImpl::RunOnWorkerThread");
    RasterFinished();
  }

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE {}

 protected:
  virtual ~RasterFinishedTaskImpl() {}

  void RasterFinished() {
    task_runner_->PostTask(FROM_HERE, on_raster_finished_callback_);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Closure on_raster_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(RasterFinishedTaskImpl);
};

}  // namespace

// This allows a micro benchmark system to run tasks with highest priority,
// since it should finish as quickly as possible.
unsigned RasterWorkerPool::kBenchmarkRasterTaskPriority = 0u;
// Task priorities that make sure raster finished tasks run before any
// remaining raster tasks.
unsigned RasterWorkerPool::kRasterFinishedTaskPriority = 1u;
unsigned RasterWorkerPool::kRasterTaskPriorityBase = 2u;

RasterWorkerPool::RasterWorkerPool() {}

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
TaskGraphRunner* RasterWorkerPool::GetTaskGraphRunner() {
  return g_task_graph_runner.Pointer();
}

// static
scoped_refptr<RasterizerTask> RasterWorkerPool::CreateRasterFinishedTask(
    base::SequencedTaskRunner* task_runner,
    const base::Closure& on_raster_finished_callback) {
  return make_scoped_refptr(
      new RasterFinishedTaskImpl(task_runner, on_raster_finished_callback));
}

// static
void RasterWorkerPool::ScheduleTasksOnOriginThread(RasterizerTaskClient* client,
                                                   TaskGraph* graph) {
  TRACE_EVENT0("cc", "Rasterizer::ScheduleTasksOnOriginThread");

  for (TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end();
       ++it) {
    TaskGraph::Node& node = *it;
    RasterizerTask* task = static_cast<RasterizerTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(client);
      task->DidSchedule();
    }
  }
}

// static
void RasterWorkerPool::InsertNodeForTask(TaskGraph* graph,
                                         RasterizerTask* task,
                                         unsigned priority,
                                         size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(),
                      graph->nodes.end(),
                      TaskGraph::Node::TaskComparator(task)) ==
         graph->nodes.end());
  graph->nodes.push_back(TaskGraph::Node(task, priority, dependencies));
}

// static
void RasterWorkerPool::InsertNodesForRasterTask(
    TaskGraph* graph,
    RasterTask* raster_task,
    const ImageDecodeTask::Vector& decode_tasks,
    unsigned priority) {
  size_t dependencies = 0u;

  // Insert image decode tasks.
  for (ImageDecodeTask::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end();
       ++it) {
    ImageDecodeTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exists in graph.
    TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(),
                     graph->nodes.end(),
                     TaskGraph::Node::TaskComparator(decode_task));
    if (decode_it == graph->nodes.end())
      InsertNodeForTask(graph, decode_task, priority, 0u);

    graph->edges.push_back(TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(graph, raster_task, priority, dependencies);
}

// static
void RasterWorkerPool::AcquireBitmapForBuffer(SkBitmap* bitmap,
                                              uint8_t* buffer,
                                              ResourceFormat buffer_format,
                                              const gfx::Size& size,
                                              int stride) {
  switch (buffer_format) {
    case RGBA_4444:
      bitmap->allocN32Pixels(size.width(), size.height());
      break;
    case RGBA_8888:
    case BGRA_8888: {
      SkImageInfo info =
          SkImageInfo::MakeN32Premul(size.width(), size.height());
      if (!stride)
        stride = info.minRowBytes();
      bitmap->installPixels(info, buffer, stride);
      break;
    }
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      NOTREACHED();
      break;
  }
}

// static
void RasterWorkerPool::ReleaseBitmapForBuffer(SkBitmap* bitmap,
                                              uint8_t* buffer,
                                              ResourceFormat buffer_format) {
  SkColorType buffer_color_type = ResourceFormatToSkColorType(buffer_format);
  if (buffer_color_type != bitmap->colorType()) {
    SkImageInfo dst_info = bitmap->info();
    dst_info.fColorType = buffer_color_type;
    // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
    // bitmap data. There will be no need to call SkAlign4 once crbug.com/293728
    // is fixed.
    const size_t dst_row_bytes = SkAlign4(dst_info.minRowBytes());
    DCHECK_EQ(0u, dst_row_bytes % 4);
    bool success = bitmap->readPixels(dst_info, buffer, dst_row_bytes, 0, 0);
    DCHECK_EQ(true, success);
  }
  bitmap->reset();
}

}  // namespace cc

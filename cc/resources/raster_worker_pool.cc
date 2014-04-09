// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <algorithm>

#include "base/atomic_sequence_num.h"
#include "base/debug/trace_event_synthetic_delay.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {
namespace {

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

class RasterTaskGraphRunner : public internal::TaskGraphRunner,
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

  size_t GetPictureCloneIndexForCurrentThread() {
    return current_tls_.Get()->picture_clone_index;
  }

 private:
  struct ThreadLocalState {
    explicit ThreadLocalState(size_t picture_clone_index)
        : picture_clone_index(picture_clone_index) {}

    size_t picture_clone_index;
  };

  // Overridden from base::DelegateSimpleThread::Delegate:
  virtual void Run() OVERRIDE {
    // Use picture clone index 0..num_threads.
    int picture_clone_index = picture_clone_index_sequence_.GetNext();
    DCHECK_LE(0, picture_clone_index);
    DCHECK_GT(RasterWorkerPool::GetNumRasterThreads(), picture_clone_index);
    current_tls_.Set(new ThreadLocalState(picture_clone_index));

    internal::TaskGraphRunner::Run();
  }

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;
  base::AtomicSequenceNumber picture_clone_index_sequence_;
  base::ThreadLocalPointer<ThreadLocalState> current_tls_;
};

base::LazyInstance<RasterTaskGraphRunner>::Leaky g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumRasterThreads = 1;

int g_num_raster_threads = 0;

class RasterFinishedWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  explicit RasterFinishedWorkerPoolTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_raster_finished_callback)
      : task_runner_(task_runner),
        on_raster_finished_callback_(on_raster_finished_callback) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread() OVERRIDE {
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
    task_runner_->PostTask(FROM_HERE, on_raster_finished_callback_);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Closure on_raster_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(RasterFinishedWorkerPoolTaskImpl);
};

class RasterRequiredForActivationFinishedWorkerPoolTaskImpl
    : public RasterFinishedWorkerPoolTaskImpl {
 public:
  RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_raster_finished_callback,
      size_t tasks_required_for_activation_count)
      : RasterFinishedWorkerPoolTaskImpl(task_runner,
                                         on_raster_finished_callback),
        tasks_required_for_activation_count_(
            tasks_required_for_activation_count) {
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->BeginParallel(
          &activation_delay_end_time_);
    }
  }

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread() OVERRIDE {
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
// This allows a micro benchmark system to run tasks with highest priority,
// since it should finish as quickly as possible.
unsigned RasterWorkerPool::kBenchmarkRasterTaskPriority = 0u;
// Task priorities that make sure raster finished tasks run before any
// remaining raster tasks.
unsigned RasterWorkerPool::kRasterFinishedTaskPriority = 2u;
unsigned RasterWorkerPool::kRasterRequiredForActivationFinishedTaskPriority =
    1u;
unsigned RasterWorkerPool::kRasterTaskPriorityBase = 3u;

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
size_t RasterWorkerPool::GetPictureCloneIndexForCurrentThread() {
  return g_task_graph_runner.Pointer()->GetPictureCloneIndexForCurrentThread();
}

// static
scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterFinishedTask(
    base::SequencedTaskRunner* task_runner,
    const base::Closure& on_raster_finished_callback) {
  return make_scoped_refptr(new RasterFinishedWorkerPoolTaskImpl(
      task_runner, on_raster_finished_callback));
}

// static
scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterRequiredForActivationFinishedTask(
    size_t tasks_required_for_activation_count,
    base::SequencedTaskRunner* task_runner,
    const base::Closure& on_raster_finished_callback) {
  return make_scoped_refptr(
      new RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
          task_runner,
          on_raster_finished_callback,
          tasks_required_for_activation_count));
}

// static
void RasterWorkerPool::ScheduleTasksOnOriginThread(
    internal::WorkerPoolTaskClient* client,
    internal::TaskGraph* graph) {
  TRACE_EVENT0("cc", "RasterWorkerPool::ScheduleTasksOnOriginThread");

  for (internal::TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end();
       ++it) {
    internal::TaskGraph::Node& node = *it;
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(client);
      task->DidSchedule();
    }
  }
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
void RasterWorkerPool::InsertNodesForRasterTask(
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

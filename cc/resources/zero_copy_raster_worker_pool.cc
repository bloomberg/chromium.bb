// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/zero_copy_raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/strings/stringprintf.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/resource.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(ResourceProvider* resource_provider,
                   const Resource* resource)
      : resource_provider_(resource_provider),
        resource_(resource),
        stride_(0),
        buffer_(resource_provider->MapImage(resource->id(), &stride_)) {}

  virtual ~RasterBufferImpl() {
    resource_provider_->UnmapImage(resource_->id());

    // This RasterBuffer implementation provides direct access to the memory
    // used by the GPU. Read lock fences are required to ensure that we're not
    // trying to map a resource that is currently in-use by the GPU.
    resource_provider_->EnableReadLockFences(resource_->id());
  }

  // Overridden from RasterBuffer:
  virtual skia::RefPtr<SkCanvas> AcquireSkCanvas() OVERRIDE {
    if (!buffer_)
      return skia::AdoptRef(SkCreateNullCanvas());

    RasterWorkerPool::AcquireBitmapForBuffer(
        &bitmap_, buffer_, resource_->format(), resource_->size(), stride_);
    return skia::AdoptRef(new SkCanvas(bitmap_));
  }
  virtual void ReleaseSkCanvas(const skia::RefPtr<SkCanvas>& canvas) OVERRIDE {
    if (!buffer_)
      return;

    RasterWorkerPool::ReleaseBitmapForBuffer(
        &bitmap_, buffer_, resource_->format());
  }

 private:
  ResourceProvider* resource_provider_;
  const Resource* resource_;
  int stride_;
  uint8_t* buffer_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
scoped_ptr<RasterWorkerPool> ZeroCopyRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider) {
  return make_scoped_ptr<RasterWorkerPool>(new ZeroCopyRasterWorkerPool(
      task_runner, task_graph_runner, resource_provider));
}

ZeroCopyRasterWorkerPool::ZeroCopyRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      raster_finished_weak_ptr_factory_(this) {
}

ZeroCopyRasterWorkerPool::~ZeroCopyRasterWorkerPool() {
}

Rasterizer* ZeroCopyRasterWorkerPool::AsRasterizer() {
  return this;
}

void ZeroCopyRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void ZeroCopyRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "ZeroCopyRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void ZeroCopyRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "ZeroCopyRasterWorkerPool::ScheduleTasks");

  if (raster_pending_.none())
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  // Mark all task sets as pending.
  raster_pending_.set();

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask> new_raster_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_raster_finished_tasks[task_set] = CreateRasterFinishedTask(
        task_runner_.get(),
        base::Bind(&ZeroCopyRasterWorkerPool::OnRasterFinished,
                   raster_finished_weak_ptr_factory_.GetWeakPtr(),
                   task_set));
  }

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_raster_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_,
                      new_raster_finished_tasks[task_set].get(),
                      kRasterFinishedTaskPriority,
                      task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  std::copy(new_raster_finished_tasks,
            new_raster_finished_tasks + kNumberOfTaskSets,
            raster_finished_tasks_);

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, "rasterizing", "state", StateAsValue());
}

void ZeroCopyRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "ZeroCopyRasterWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

scoped_ptr<RasterBuffer> ZeroCopyRasterWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  // RasterBuffer implementation depends on an image having been acquired for
  // the resource.
  resource_provider_->AcquireImage(resource->id());

  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(resource_provider_, resource));
}

void ZeroCopyRasterWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void ZeroCopyRasterWorkerPool::OnRasterFinished(TaskSet task_set) {
  TRACE_EVENT1(
      "cc", "ZeroCopyRasterWorkerPool::OnRasterFinished", "task_set", task_set);

  DCHECK(raster_pending_[task_set]);
  raster_pending_[task_set] = false;
  if (raster_pending_.any()) {
    TRACE_EVENT_ASYNC_STEP_INTO1(
        "cc", "ScheduledTasks", this, "rasterizing", "state", StateAsValue());
  } else {
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  }
  client_->DidFinishRunningTasks(task_set);
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
ZeroCopyRasterWorkerPool::StateAsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();

  state->BeginArray("tasks_pending");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendBoolean(raster_pending_[task_set]);
  state->EndArray();
  return state;
}

}  // namespace cc

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/bitmap_tile_task_worker_pool.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/resource.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(ResourceProvider* resource_provider,
                   const Resource* resource)
      : lock_(resource_provider, resource->id()), resource_(resource) {}

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& rect,
                float scale) override {
    TileTaskWorkerPool::PlaybackToMemory(lock_.sk_bitmap().getPixels(),
                                         resource_->format(), resource_->size(),
                                         0, raster_source, rect, scale);
  }

 private:
  ResourceProvider::ScopedWriteLockSoftware lock_;
  const Resource* resource_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
scoped_ptr<TileTaskWorkerPool> BitmapTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider) {
  return make_scoped_ptr<TileTaskWorkerPool>(new BitmapTileTaskWorkerPool(
      task_runner, task_graph_runner, resource_provider));
}

BitmapTileTaskWorkerPool::BitmapTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      task_set_finished_weak_ptr_factory_(this) {
}

BitmapTileTaskWorkerPool::~BitmapTileTaskWorkerPool() {
}

TileTaskRunner* BitmapTileTaskWorkerPool::AsTileTaskRunner() {
  return this;
}

void BitmapTileTaskWorkerPool::SetClient(TileTaskRunnerClient* client) {
  client_ = client;
}

void BitmapTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "BitmapTileTaskWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void BitmapTileTaskWorkerPool::ScheduleTasks(TileTaskQueue* queue) {
  TRACE_EVENT0("cc", "BitmapTileTaskWorkerPool::ScheduleTasks");

  if (tasks_pending_.none())
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  // Mark all task sets as pending.
  tasks_pending_.set();

  unsigned priority = kTileTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<TileTask> new_task_set_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_task_set_finished_tasks[task_set] = CreateTaskSetFinishedTask(
        task_runner_.get(),
        base::Bind(&BitmapTileTaskWorkerPool::OnTaskSetFinished,
                   task_set_finished_weak_ptr_factory_.GetWeakPtr(), task_set));
  }

  for (TileTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end(); ++it) {
    const TileTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_task_set_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_, new_task_set_finished_tasks[task_set].get(),
                      kTaskSetFinishedTaskPriorityBase + task_set,
                      task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  std::copy(new_task_set_finished_tasks,
            new_task_set_finished_tasks + kNumberOfTaskSets,
            task_set_finished_tasks_);

  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               StateAsValue());
}

void BitmapTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "BitmapTileTaskWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end(); ++it) {
    TileTask* task = static_cast<TileTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

ResourceFormat BitmapTileTaskWorkerPool::GetResourceFormat() {
  return resource_provider_->best_texture_format();
}

scoped_ptr<RasterBuffer> BitmapTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(resource_provider_, resource));
}

void BitmapTileTaskWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void BitmapTileTaskWorkerPool::OnTaskSetFinished(TaskSet task_set) {
  TRACE_EVENT1("cc", "BitmapTileTaskWorkerPool::OnTaskSetFinished", "task_set",
               task_set);

  DCHECK(tasks_pending_[task_set]);
  tasks_pending_[task_set] = false;
  if (tasks_pending_.any()) {
    TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running",
                                 "state", StateAsValue());
  } else {
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  }
  client_->DidFinishRunningTileTasks(task_set);
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
BitmapTileTaskWorkerPool::StateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->BeginArray("tasks_pending");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendBoolean(tasks_pending_[task_set]);
  state->EndArray();
  return state;
}

}  // namespace cc

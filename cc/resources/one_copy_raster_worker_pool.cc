// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/one_copy_raster_worker_pool.h"

#include <algorithm>
#include <limits>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/strings/stringprintf.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/scoped_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(OneCopyRasterWorkerPool* worker_pool,
                   ResourceProvider* resource_provider,
                   ResourcePool* resource_pool,
                   const Resource* resource)
      : worker_pool_(worker_pool),
        resource_provider_(resource_provider),
        resource_pool_(resource_pool),
        resource_(resource),
        raster_resource_(resource_pool->AcquireResource(resource->size())),
        lock_(new ResourceProvider::ScopedWriteLockGpuMemoryBuffer(
            resource_provider_,
            raster_resource_->id())),
        sequence_(0) {}

  ~RasterBufferImpl() override {
    // Release write lock in case a copy was never scheduled.
    lock_.reset();

    // Make sure any scheduled copy operations are issued before we release the
    // raster resource.
    if (sequence_)
      worker_pool_->AdvanceLastIssuedCopyTo(sequence_);

    // Return raster resource to pool so it can be used by another RasterBuffer
    // instance.
    resource_pool_->ReleaseResource(raster_resource_.Pass());
  }

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& rect,
                float scale,
                RenderingStatsInstrumentation* stats) override {
    gfx::GpuMemoryBuffer* gpu_memory_buffer = lock_->GetGpuMemoryBuffer();
    if (!gpu_memory_buffer)
      return;

    RasterWorkerPool::PlaybackToMemory(gpu_memory_buffer->Map(),
                                       raster_resource_->format(),
                                       raster_resource_->size(),
                                       gpu_memory_buffer->GetStride(),
                                       raster_source,
                                       rect,
                                       scale,
                                       stats);
    gpu_memory_buffer->Unmap();

    sequence_ = worker_pool_->ScheduleCopyOnWorkerThread(
        lock_.Pass(), raster_resource_.get(), resource_);
  }

 private:
  OneCopyRasterWorkerPool* worker_pool_;
  ResourceProvider* resource_provider_;
  ResourcePool* resource_pool_;
  const Resource* resource_;
  scoped_ptr<ScopedResource> raster_resource_;
  scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> lock_;
  CopySequenceNumber sequence_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

// Flush interval when performing copy operations.
const int kCopyFlushPeriod = 4;

}  // namespace

OneCopyRasterWorkerPool::CopyOperation::CopyOperation(
    scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> write_lock,
    ResourceProvider::ResourceId src,
    ResourceProvider::ResourceId dst)
    : write_lock(write_lock.Pass()), src(src), dst(dst) {
}

OneCopyRasterWorkerPool::CopyOperation::~CopyOperation() {
}

// static
scoped_ptr<RasterWorkerPool> OneCopyRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool) {
  return make_scoped_ptr<RasterWorkerPool>(
      new OneCopyRasterWorkerPool(task_runner,
                                  task_graph_runner,
                                  context_provider,
                                  resource_provider,
                                  resource_pool));
}

OneCopyRasterWorkerPool::OneCopyRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      resource_pool_(resource_pool),
      last_issued_copy_operation_(0),
      last_flushed_copy_operation_(0),
      next_copy_operation_sequence_(1),
      weak_ptr_factory_(this),
      raster_finished_weak_ptr_factory_(this) {
  DCHECK(context_provider_);
}

OneCopyRasterWorkerPool::~OneCopyRasterWorkerPool() {
}

Rasterizer* OneCopyRasterWorkerPool::AsRasterizer() {
  return this;
}

void OneCopyRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void OneCopyRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "OneCopyRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void OneCopyRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "OneCopyRasterWorkerPool::ScheduleTasks");

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
        base::Bind(&OneCopyRasterWorkerPool::OnRasterFinished,
                   raster_finished_weak_ptr_factory_.GetWeakPtr(),
                   task_set));
  }

  resource_pool_->CheckBusyResources();

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

  resource_pool_->ReduceResourceUsage();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, "rasterizing", "state", StateAsValue());
}

void OneCopyRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "OneCopyRasterWorkerPool::CheckForCompletedTasks");

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

scoped_ptr<RasterBuffer> OneCopyRasterWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  DCHECK_EQ(resource->format(), resource_pool_->resource_format());
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(this, resource_provider_, resource_pool_, resource));
}

void OneCopyRasterWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

CopySequenceNumber OneCopyRasterWorkerPool::ScheduleCopyOnWorkerThread(
    scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> write_lock,
    const Resource* src,
    const Resource* dst) {
  CopySequenceNumber sequence;

  {
    base::AutoLock lock(lock_);

    sequence = next_copy_operation_sequence_++;

    pending_copy_operations_.push_back(make_scoped_ptr(
        new CopyOperation(write_lock.Pass(), src->id(), dst->id())));
  }

  // Post task that will advance last flushed copy operation to |sequence|
  // if we have reached the flush period.
  if ((sequence % kCopyFlushPeriod) == 0) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&OneCopyRasterWorkerPool::AdvanceLastFlushedCopyTo,
                   weak_ptr_factory_.GetWeakPtr(),
                   sequence));
  }

  return sequence;
}

void OneCopyRasterWorkerPool::AdvanceLastIssuedCopyTo(
    CopySequenceNumber sequence) {
  if (last_issued_copy_operation_ >= sequence)
    return;

  IssueCopyOperations(sequence - last_issued_copy_operation_);
  last_issued_copy_operation_ = sequence;
}

void OneCopyRasterWorkerPool::AdvanceLastFlushedCopyTo(
    CopySequenceNumber sequence) {
  if (last_flushed_copy_operation_ >= sequence)
    return;

  AdvanceLastIssuedCopyTo(sequence);

  // Flush all issued copy operations.
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  last_flushed_copy_operation_ = last_issued_copy_operation_;
}

void OneCopyRasterWorkerPool::OnRasterFinished(TaskSet task_set) {
  TRACE_EVENT1(
      "cc", "OneCopyRasterWorkerPool::OnRasterFinished", "task_set", task_set);

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

void OneCopyRasterWorkerPool::IssueCopyOperations(int64 count) {
  TRACE_EVENT1(
      "cc", "OneCopyRasterWorkerPool::IssueCopyOperations", "count", count);

  CopyOperation::Deque copy_operations;

  {
    base::AutoLock lock(lock_);

    for (int64 i = 0; i < count; ++i) {
      DCHECK(!pending_copy_operations_.empty());
      copy_operations.push_back(pending_copy_operations_.take_front());
    }
  }

  while (!copy_operations.empty()) {
    scoped_ptr<CopyOperation> copy_operation = copy_operations.take_front();

    // Remove the write lock.
    copy_operation->write_lock.reset();

    // Copy contents of source resource to destination resource.
    resource_provider_->CopyResource(copy_operation->src, copy_operation->dst);
  }
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
OneCopyRasterWorkerPool::StateAsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();

  state->BeginArray("tasks_pending");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendBoolean(raster_pending_[task_set]);
  state->EndArray();
  state->BeginDictionary("staging_state");
  StagingStateAsValueInto(state.get());
  state->EndDictionary();

  return state;
}
void OneCopyRasterWorkerPool::StagingStateAsValueInto(
    base::debug::TracedValue* staging_state) const {
  staging_state->SetInteger("staging_resource_count",
                            resource_pool_->total_resource_count());
  staging_state->SetInteger("bytes_used_for_staging_resources",
                            resource_pool_->total_memory_usage_bytes());
  staging_state->SetInteger("pending_copy_count",
                            resource_pool_->total_resource_count() -
                                resource_pool_->acquired_resource_count());
  staging_state->SetInteger("bytes_pending_copy",
                            resource_pool_->total_memory_usage_bytes() -
                                resource_pool_->acquired_memory_usage_bytes());
}

}  // namespace cc

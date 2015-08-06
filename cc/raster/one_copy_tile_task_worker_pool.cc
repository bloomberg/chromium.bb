// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/one_copy_tile_task_worker_pool.h"

#include <algorithm>
#include <limits>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/raster/raster_buffer.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/scoped_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(OneCopyTileTaskWorkerPool* worker_pool,
                   ResourceProvider* resource_provider,
                   ResourcePool* resource_pool,
                   ResourceFormat resource_format,
                   const Resource* output_resource,
                   uint64_t previous_content_id)
      : worker_pool_(worker_pool),
        resource_provider_(resource_provider),
        resource_pool_(resource_pool),
        output_resource_(output_resource),
        raster_content_id_(0),
        sequence_(0) {
    if (worker_pool->have_persistent_gpu_memory_buffers() &&
        previous_content_id) {
      raster_resource_ =
          resource_pool->TryAcquireResourceWithContentId(previous_content_id);
    }
    if (raster_resource_) {
      raster_content_id_ = previous_content_id;
      DCHECK_EQ(resource_format, raster_resource_->format());
      DCHECK_EQ(output_resource->size().ToString(),
                raster_resource_->size().ToString());
    } else {
      raster_resource_ = resource_pool->AcquireResource(output_resource->size(),
                                                        resource_format);
    }

    lock_.reset(new ResourceProvider::ScopedWriteLockGpuMemoryBuffer(
        resource_provider_, raster_resource_->id()));
  }

  ~RasterBufferImpl() override {
    // Release write lock in case a copy was never scheduled.
    lock_.reset();

    // Make sure any scheduled copy operations are issued before we release the
    // raster resource.
    if (sequence_)
      worker_pool_->AdvanceLastIssuedCopyTo(sequence_);

    // Return resources to pool so they can be used by another RasterBuffer
    // instance.
    resource_pool_->ReleaseResource(raster_resource_.Pass(),
                                    raster_content_id_);
  }

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                float scale,
                bool include_images) override {
    // If there's a raster_content_id_, we are reusing a resource with that
    // content id.
    bool reusing_raster_resource = raster_content_id_ != 0;
    sequence_ = worker_pool_->PlaybackAndScheduleCopyOnWorkerThread(
        reusing_raster_resource, lock_.Pass(), raster_resource_.get(),
        output_resource_, raster_source, raster_full_rect, raster_dirty_rect,
        scale, include_images);
    // Store the content id of the resource to return to the pool.
    raster_content_id_ = new_content_id;
  }

 private:
  OneCopyTileTaskWorkerPool* worker_pool_;
  ResourceProvider* resource_provider_;
  ResourcePool* resource_pool_;
  const Resource* output_resource_;
  uint64_t raster_content_id_;
  scoped_ptr<ScopedResource> raster_resource_;
  scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> lock_;
  CopySequenceNumber sequence_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

// Number of in-flight copy operations to allow.
const int kMaxCopyOperations = 32;

// Delay been checking for copy operations to complete.
const int kCheckForCompletedCopyOperationsTickRateMs = 1;

// Number of failed attempts to allow before we perform a check that will
// wait for copy operations to complete if needed.
const int kFailedAttemptsBeforeWaitIfNeeded = 256;

// 4MiB is the size of 4 512x512 tiles, which has proven to be a good
// default batch size for copy operations.
const int kMaxBytesPerCopyOperation = 1024 * 1024 * 4;

}  // namespace

OneCopyTileTaskWorkerPool::CopyOperation::CopyOperation(
    scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> src_write_lock,
    const Resource* src,
    const Resource* dst,
    const gfx::Rect& rect)
    : src_write_lock(src_write_lock.Pass()), src(src), dst(dst), rect(rect) {
}

OneCopyTileTaskWorkerPool::CopyOperation::~CopyOperation() {
}

// static
scoped_ptr<TileTaskWorkerPool> OneCopyTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool,
    int max_copy_texture_chromium_size,
    bool have_persistent_gpu_memory_buffers) {
  return make_scoped_ptr<TileTaskWorkerPool>(new OneCopyTileTaskWorkerPool(
      task_runner, task_graph_runner, context_provider, resource_provider,
      resource_pool, max_copy_texture_chromium_size,
      have_persistent_gpu_memory_buffers));
}

OneCopyTileTaskWorkerPool::OneCopyTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool,
    int max_copy_texture_chromium_size,
    bool have_persistent_gpu_memory_buffers)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      resource_pool_(resource_pool),
      max_bytes_per_copy_operation_(
          max_copy_texture_chromium_size
              ? std::min(kMaxBytesPerCopyOperation,
                         max_copy_texture_chromium_size)
              : kMaxBytesPerCopyOperation),
      have_persistent_gpu_memory_buffers_(have_persistent_gpu_memory_buffers),
      last_issued_copy_operation_(0),
      last_flushed_copy_operation_(0),
      lock_(),
      copy_operation_count_cv_(&lock_),
      bytes_scheduled_since_last_flush_(0),
      issued_copy_operation_count_(0),
      next_copy_operation_sequence_(1),
      check_for_completed_copy_operations_pending_(false),
      shutdown_(false),
      weak_ptr_factory_(this),
      task_set_finished_weak_ptr_factory_(this) {
  DCHECK(context_provider_);
}

OneCopyTileTaskWorkerPool::~OneCopyTileTaskWorkerPool() {
  DCHECK_EQ(pending_copy_operations_.size(), 0u);
}

TileTaskRunner* OneCopyTileTaskWorkerPool::AsTileTaskRunner() {
  return this;
}

void OneCopyTileTaskWorkerPool::SetClient(TileTaskRunnerClient* client) {
  client_ = client;
}

void OneCopyTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::Shutdown");

  {
    base::AutoLock lock(lock_);

    shutdown_ = true;
    copy_operation_count_cv_.Signal();
  }

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void OneCopyTileTaskWorkerPool::ScheduleTasks(TileTaskQueue* queue) {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::ScheduleTasks");

#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK(!shutdown_);
  }
#endif

  if (tasks_pending_.none())
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  // Mark all task sets as pending.
  tasks_pending_.set();

  size_t priority = kTileTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<TileTask> new_task_set_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_task_set_finished_tasks[task_set] = CreateTaskSetFinishedTask(
        task_runner_.get(),
        base::Bind(&OneCopyTileTaskWorkerPool::OnTaskSetFinished,
                   task_set_finished_weak_ptr_factory_.GetWeakPtr(), task_set));
  }

  resource_pool_->CheckBusyResources(false);

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

  resource_pool_->ReduceResourceUsage();

  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               StateAsValue());
}

void OneCopyTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::CheckForCompletedTasks");

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

ResourceFormat OneCopyTileTaskWorkerPool::GetResourceFormat() const {
  return resource_provider_->best_texture_format();
}

bool OneCopyTileTaskWorkerPool::GetResourceRequiresSwizzle() const {
  return !PlatformColor::SameComponentOrder(GetResourceFormat());
}

scoped_ptr<RasterBuffer> OneCopyTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  // TODO(danakj): If resource_content_id != 0, we only need to copy/upload
  // the dirty rect.
  DCHECK_EQ(resource->format(), resource_provider_->best_texture_format());
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(this, resource_provider_, resource_pool_,
                           resource_provider_->best_texture_format(), resource,
                           previous_content_id));
}

void OneCopyTileTaskWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

CopySequenceNumber
OneCopyTileTaskWorkerPool::PlaybackAndScheduleCopyOnWorkerThread(
    bool reusing_raster_resource,
    scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer>
        raster_resource_write_lock,
    const Resource* raster_resource,
    const Resource* output_resource,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    float scale,
    bool include_images) {
  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      raster_resource_write_lock->GetGpuMemoryBuffer();
  if (gpu_memory_buffer) {
    void* data = NULL;
    bool rv = gpu_memory_buffer->Map(&data);
    DCHECK(rv);
    int stride;
    gpu_memory_buffer->GetStride(&stride);
    // TileTaskWorkerPool::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(stride, 0);

    gfx::Rect playback_rect = raster_full_rect;
    if (reusing_raster_resource) {
      playback_rect.Intersect(raster_dirty_rect);
    }
    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";
    TileTaskWorkerPool::PlaybackToMemory(
        data, raster_resource->format(), raster_resource->size(),
        static_cast<size_t>(stride), raster_source, raster_full_rect,
        playback_rect, scale, include_images);
    gpu_memory_buffer->Unmap();
  }

  base::AutoLock lock(lock_);

  CopySequenceNumber sequence = 0;
  int bytes_per_row = (BitsPerPixel(raster_resource->format()) *
                       raster_resource->size().width()) /
                      8;
  int chunk_size_in_rows =
      std::max(1, max_bytes_per_copy_operation_ / bytes_per_row);
  // Align chunk size to 4. Required to support compressed texture formats.
  chunk_size_in_rows = MathUtil::RoundUp(chunk_size_in_rows, 4);
  int y = 0;
  int height = raster_resource->size().height();
  while (y < height) {
    int failed_attempts = 0;
    while ((pending_copy_operations_.size() + issued_copy_operation_count_) >=
           kMaxCopyOperations) {
      // Ignore limit when shutdown is set.
      if (shutdown_)
        break;

      ++failed_attempts;

      // Schedule a check that will also wait for operations to complete
      // after too many failed attempts.
      bool wait_if_needed = failed_attempts > kFailedAttemptsBeforeWaitIfNeeded;

      // Schedule a check for completed copy operations if too many operations
      // are currently in-flight.
      ScheduleCheckForCompletedCopyOperationsWithLockAcquired(wait_if_needed);

      {
        TRACE_EVENT0("cc", "WaitingForCopyOperationsToComplete");

        // Wait for in-flight copy operations to drop below limit.
        copy_operation_count_cv_.Wait();
      }
    }

    // There may be more work available, so wake up another worker thread.
    copy_operation_count_cv_.Signal();

    // Copy at most |chunk_size_in_rows|.
    int rows_to_copy = std::min(chunk_size_in_rows, height - y);
    DCHECK_GT(rows_to_copy, 0);

    // |raster_resource_write_lock| is passed to the first copy operation as it
    // needs to be released before we can issue a copy.
    pending_copy_operations_.push_back(make_scoped_ptr(new CopyOperation(
        raster_resource_write_lock.Pass(), raster_resource, output_resource,
        gfx::Rect(0, y, raster_resource->size().width(), rows_to_copy))));
    y += rows_to_copy;

    // Acquire a sequence number for this copy operation.
    sequence = next_copy_operation_sequence_++;

    // Increment |bytes_scheduled_since_last_flush_| by the amount of memory
    // used for this copy operation.
    bytes_scheduled_since_last_flush_ += rows_to_copy * bytes_per_row;

    // Post task that will advance last flushed copy operation to |sequence|
    // when |bytes_scheduled_since_last_flush_| has reached
    // |max_bytes_per_copy_operation_|.
    if (bytes_scheduled_since_last_flush_ >= max_bytes_per_copy_operation_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&OneCopyTileTaskWorkerPool::AdvanceLastFlushedCopyTo,
                     weak_ptr_factory_.GetWeakPtr(), sequence));
      bytes_scheduled_since_last_flush_ = 0;
    }
  }

  return sequence;
}

void OneCopyTileTaskWorkerPool::AdvanceLastIssuedCopyTo(
    CopySequenceNumber sequence) {
  if (last_issued_copy_operation_ >= sequence)
    return;

  IssueCopyOperations(sequence - last_issued_copy_operation_);
  last_issued_copy_operation_ = sequence;
}

void OneCopyTileTaskWorkerPool::AdvanceLastFlushedCopyTo(
    CopySequenceNumber sequence) {
  if (last_flushed_copy_operation_ >= sequence)
    return;

  AdvanceLastIssuedCopyTo(sequence);

  // Flush all issued copy operations.
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  last_flushed_copy_operation_ = last_issued_copy_operation_;
}

void OneCopyTileTaskWorkerPool::OnTaskSetFinished(TaskSet task_set) {
  TRACE_EVENT1("cc", "OneCopyTileTaskWorkerPool::OnTaskSetFinished", "task_set",
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

void OneCopyTileTaskWorkerPool::IssueCopyOperations(int64 count) {
  TRACE_EVENT1("cc", "OneCopyTileTaskWorkerPool::IssueCopyOperations", "count",
               count);

  CopyOperation::Deque copy_operations;

  {
    base::AutoLock lock(lock_);

    for (int64 i = 0; i < count; ++i) {
      DCHECK(!pending_copy_operations_.empty());
      copy_operations.push_back(pending_copy_operations_.take_front());
    }

    // Increment |issued_copy_operation_count_| to reflect the transition of
    // copy operations from "pending" to "issued" state.
    issued_copy_operation_count_ += copy_operations.size();
  }

  while (!copy_operations.empty()) {
    scoped_ptr<CopyOperation> copy_operation = copy_operations.take_front();

    // Remove the write lock.
    copy_operation->src_write_lock.reset();

    // Copy contents of source resource to destination resource.
    resource_provider_->CopyResource(copy_operation->src->id(),
                                     copy_operation->dst->id(),
                                     copy_operation->rect);
  }
}

void OneCopyTileTaskWorkerPool::
    ScheduleCheckForCompletedCopyOperationsWithLockAcquired(
        bool wait_if_needed) {
  lock_.AssertAcquired();

  if (check_for_completed_copy_operations_pending_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();

  // Schedule a check for completed copy operations as soon as possible but
  // don't allow two consecutive checks to be scheduled to run less than the
  // tick rate apart.
  base::TimeTicks next_check_for_completed_copy_operations_time =
      std::max(last_check_for_completed_copy_operations_time_ +
                   base::TimeDelta::FromMilliseconds(
                       kCheckForCompletedCopyOperationsTickRateMs),
               now);

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OneCopyTileTaskWorkerPool::CheckForCompletedCopyOperations,
                 weak_ptr_factory_.GetWeakPtr(), wait_if_needed),
      next_check_for_completed_copy_operations_time - now);

  last_check_for_completed_copy_operations_time_ =
      next_check_for_completed_copy_operations_time;
  check_for_completed_copy_operations_pending_ = true;
}

void OneCopyTileTaskWorkerPool::CheckForCompletedCopyOperations(
    bool wait_if_needed) {
  TRACE_EVENT1("cc",
               "OneCopyTileTaskWorkerPool::CheckForCompletedCopyOperations",
               "wait_if_needed", wait_if_needed);

  resource_pool_->CheckBusyResources(wait_if_needed);

  {
    base::AutoLock lock(lock_);

    DCHECK(check_for_completed_copy_operations_pending_);
    check_for_completed_copy_operations_pending_ = false;

    // The number of busy resources in the pool reflects the number of issued
    // copy operations that have not yet completed.
    issued_copy_operation_count_ = resource_pool_->busy_resource_count();

    // There may be work blocked on too many in-flight copy operations, so wake
    // up a worker thread.
    copy_operation_count_cv_.Signal();
  }
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
OneCopyTileTaskWorkerPool::StateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->BeginArray("tasks_pending");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendBoolean(tasks_pending_[task_set]);
  state->EndArray();
  state->BeginDictionary("staging_state");
  StagingStateAsValueInto(state.get());
  state->EndDictionary();

  return state;
}

void OneCopyTileTaskWorkerPool::StagingStateAsValueInto(
    base::trace_event::TracedValue* staging_state) const {
  staging_state->SetInteger(
      "staging_resource_count",
      static_cast<int>(resource_pool_->total_resource_count()));
  staging_state->SetInteger(
      "bytes_used_for_staging_resources",
      static_cast<int>(resource_pool_->total_memory_usage_bytes()));
  staging_state->SetInteger(
      "pending_copy_count",
      static_cast<int>(resource_pool_->total_resource_count() -
                       resource_pool_->acquired_resource_count()));
  staging_state->SetInteger(
      "bytes_pending_copy",
      static_cast<int>(resource_pool_->total_memory_usage_bytes() -
                       resource_pool_->acquired_memory_usage_bytes()));
}

}  // namespace cc

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/pixel_buffer_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/resources/resource.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

namespace {

class PixelBufferWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(bool was_canceled, bool needs_upload)> Reply;

  PixelBufferWorkerPoolTaskImpl(internal::RasterWorkerPoolTask* task,
                                uint8_t* buffer,
                                const Reply& reply)
      : task_(task),
        buffer_(buffer),
        reply_(reply),
        needs_upload_(false) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    // |buffer_| can be NULL in lost context situations.
    if (!buffer_) {
      // |needs_upload_| still needs to be true as task has not
      // been canceled.
      needs_upload_ = true;
      return;
    }
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     task_->resource()->size().width(),
                     task_->resource()->size().height());
    bitmap.setPixels(buffer_);
    SkDevice device(bitmap);
    needs_upload_ = task_->RunOnWorkerThread(&device, thread_index);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    // |needs_upload_| must be be false if task didn't run.
    DCHECK(HasFinishedRunning() || !needs_upload_);
    reply_.Run(!HasFinishedRunning(), needs_upload_);
  }

 private:
  virtual ~PixelBufferWorkerPoolTaskImpl() {}

  scoped_refptr<internal::RasterWorkerPoolTask> task_;
  uint8_t* buffer_;
  const Reply reply_;
  bool needs_upload_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferWorkerPoolTaskImpl);
};

// If we raster too fast we become upload bound, and pending
// uploads consume memory. For maximum upload throughput, we would
// want to allow for upload_throughput * pipeline_time of pending
// uploads, after which we are just wasting memory. Since we don't
// know our upload throughput yet, this just caps our memory usage.
#if defined(OS_ANDROID)
// For reference Nexus10 can upload 1MB in about 2.5ms.
const size_t kMaxBytesUploadedPerMs = (2 * 1024 * 1024) / 5;
#else
// For reference Chromebook Pixel can upload 1MB in about 0.5ms.
const size_t kMaxBytesUploadedPerMs = 1024 * 1024 * 2;
#endif

// Assuming a two frame deep pipeline.
const size_t kMaxPendingUploadBytes = 16 * 2 * kMaxBytesUploadedPerMs;

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxPendingRasterBytes =
    kMaxBytesUploadedPerMs * kCheckForCompletedRasterTasksDelayMs;

}  // namespace

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    ResourceProvider* resource_provider,
    size_t num_threads) : RasterWorkerPool(resource_provider, num_threads),
                          shutdown_(false),
                          bytes_pending_upload_(0),
                          has_performed_uploads_since_last_flush_(false),
                          check_for_completed_raster_tasks_pending_(false) {
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK(shutdown_);
  DCHECK(!check_for_completed_raster_tasks_pending_);
  DCHECK_EQ(0u, pixel_buffer_tasks_.size());
  DCHECK_EQ(0u, tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_tasks_.size());
}

void PixelBufferRasterWorkerPool::Shutdown() {
  shutdown_ = true;
  RasterWorkerPool::Shutdown();
  CheckForCompletedRasterTasks();
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    internal::WorkerPoolTask* pixel_buffer_task = it->second.get();

    // All inactive tasks needs to be canceled.
    if (!pixel_buffer_task && !task->HasFinishedRunning()) {
      task->DidRun(true);
      completed_tasks_.push_back(task);
    }
  }
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  // Build new pixel buffer task set.
  TaskMap new_pixel_buffer_tasks;
  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(new_pixel_buffer_tasks.find(task) == new_pixel_buffer_tasks.end());
    DCHECK(!task->HasCompleted());

    // Use existing pixel buffer task if available.
    TaskMap::iterator pixel_buffer_it = pixel_buffer_tasks_.find(task);
    if (pixel_buffer_it == pixel_buffer_tasks_.end()) {
      new_pixel_buffer_tasks[task] = NULL;
      continue;
    }

    new_pixel_buffer_tasks[task] = pixel_buffer_it->second;
    pixel_buffer_tasks_.erase(task);
  }

  // Transfer remaining pixel buffer tasks to |new_pixel_buffer_tasks|
  // and cancel all remaining inactive tasks.
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    internal::WorkerPoolTask* pixel_buffer_task = it->second.get();

    // Move task to |new_pixel_buffer_tasks|
    new_pixel_buffer_tasks[task] = pixel_buffer_task;

    // Inactive task can be canceled.
    if (!pixel_buffer_task && !task->HasFinishedRunning()) {
      task->DidRun(true);
      DCHECK(std::find(completed_tasks_.begin(),
                       completed_tasks_.end(),
                       task) == completed_tasks_.end());
      completed_tasks_.push_back(task);
    }
  }

  pixel_buffer_tasks_.swap(new_pixel_buffer_tasks);

  // This will schedule more tasks after checking for completed raster
  // tasks. It's worth checking for completed tasks when ScheduleTasks()
  // is called as priorities might have changed and this allows us to
  // schedule as many new top priority tasks as possible.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  FlushUploads();

  while (!completed_tasks_.empty()) {
    internal::RasterWorkerPoolTask* task = completed_tasks_.front().get();
    DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

    pixel_buffer_tasks_.erase(task);

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();

    completed_tasks_.pop_front();
  }
}

void PixelBufferRasterWorkerPool::OnRasterTasksFinished() {
  // Call CheckForCompletedTasks() when we've finished running all raster
  // tasks needed since last time ScheduleMoreTasks() was called. This
  // reduces latency when processing only a small number of raster tasks.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::FlushUploads() {
  if (!has_performed_uploads_since_last_flush_)
    return;

  resource_provider()->ShallowFlushIfSupported();
  has_performed_uploads_since_last_flush_ = false;
}

void PixelBufferRasterWorkerPool::CheckForCompletedUploads() {
  TaskDeque tasks_with_completed_uploads;

  // First check if any have completed.
  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_pending_upload_.front().get();

    // Uploads complete in the order they are issued.
    if (!resource_provider()->DidSetPixelsComplete(task->resource()->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client());
  bool should_force_some_uploads_to_complete =
      shutdown_ || client()->ShouldForceTasksRequiredForActivationToComplete();

  if (should_force_some_uploads_to_complete) {
    TaskDeque tasks_with_uploads_to_force;
    TaskDeque::iterator it = tasks_with_pending_upload_.begin();
    while (it != tasks_with_pending_upload_.end()) {
      internal::RasterWorkerPoolTask* task = it->get();
      DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

      // Force all uploads required for activation to complete.
      // During shutdown, force all pending uploads to complete.
      if (shutdown_ || IsRasterTaskRequiredForActivation(task)) {
        tasks_with_uploads_to_force.push_back(task);
        tasks_with_completed_uploads.push_back(task);
        it = tasks_with_pending_upload_.erase(it);
        continue;
      }

      ++it;
    }

    // Force uploads in reverse order. Since forcing can cause a wait on
    // all previous uploads, we would rather wait only once downstream.
    for (TaskDeque::reverse_iterator it = tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      resource_provider()->ForceSetPixelsToComplete((*it)->resource()->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_tasks_|.
  while (!tasks_with_completed_uploads.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_completed_uploads.front().get();

    // It's now safe to release the pixel buffer and the shared memory.
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    task->DidRun(false);

    DCHECK(std::find(completed_tasks_.begin(),
                     completed_tasks_.end(),
                     task) == completed_tasks_.end());
    completed_tasks_.push_back(task);

    tasks_with_completed_uploads.pop_front();
  }
}

void PixelBufferRasterWorkerPool::ScheduleCheckForCompletedRasterTasks() {
  if (check_for_completed_raster_tasks_pending_)
    return;

  check_for_completed_raster_tasks_callback_.Reset(
      base::Bind(&PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks,
                 base::Unretained(this)));
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      check_for_completed_raster_tasks_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kCheckForCompletedRasterTasksDelayMs));
  check_for_completed_raster_tasks_pending_ = true;
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks() {
  TRACE_EVENT0(
      "cc", "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;

  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  FlushUploads();

  ScheduleMoreTasks();

  // Make sure another check for completed uploads is scheduled
  // while there is still pending uploads left.
  if (!tasks_with_pending_upload_.empty())
    ScheduleCheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  size_t bytes_pending_upload = bytes_pending_upload_;
  size_t bytes_pending_raster = 0;

  RasterTaskGraph graph;
  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();

    // |pixel_buffer_tasks_| contains all tasks that have not yet completed.
    TaskMap::iterator pixel_buffer_it = pixel_buffer_tasks_.find(task);
    if (pixel_buffer_it == pixel_buffer_tasks_.end())
      continue;

    // HasFinishedRunning() will return true when set pixels has completed.
    if (task->HasFinishedRunning()) {
      DCHECK(std::find(completed_tasks_.begin(),
                       completed_tasks_.end(),
                       task) != completed_tasks_.end());
      continue;
    }

    // All raster tasks need to be throttled by bytes of pending uploads.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > kMaxPendingUploadBytes)
      break;

    internal::WorkerPoolTask* pixel_buffer_task = pixel_buffer_it->second.get();

    // If raster has finished, just update |bytes_pending_upload|.
    if (pixel_buffer_task && pixel_buffer_task->HasCompleted()) {
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on bytes pending if raster has not
    // finished.
    size_t new_bytes_pending_raster = bytes_pending_raster;
    new_bytes_pending_raster += task->resource()->bytes();
    if (new_bytes_pending_raster > kMaxPendingRasterBytes)
      break;

    // Update both |bytes_pending_raster| and |bytes_pending_upload|
    // now that task has cleared all throttling limits.
    bytes_pending_raster = new_bytes_pending_raster;
    bytes_pending_upload = new_bytes_pending_upload;

    // Use existing pixel buffer task if available.
    if (pixel_buffer_task) {
      graph.InsertRasterTask(pixel_buffer_task, task->dependencies());
      continue;
    }

    // Request a pixel buffer. This will reserve shared memory.
    resource_provider()->AcquirePixelBuffer(task->resource()->id());

    // MapPixelBuffer() returns NULL if context was lost at the time
    // AcquirePixelBuffer() was called. For simplicity we still post
    // a raster task that is essentially a noop in these situations.
    uint8* buffer = resource_provider()->MapPixelBuffer(
        task->resource()->id());

    scoped_refptr<internal::WorkerPoolTask> new_pixel_buffer_task(
        new PixelBufferWorkerPoolTaskImpl(
            task,
            buffer,
            base::Bind(&PixelBufferRasterWorkerPool::OnRasterTaskCompleted,
                       base::Unretained(this),
                       make_scoped_refptr(task))));
    pixel_buffer_tasks_[task] = new_pixel_buffer_task;
    graph.InsertRasterTask(new_pixel_buffer_task.get(), task->dependencies());
  }

  SetRasterTaskGraph(&graph);

  // At least one task that could need an upload is now pending, schedule
  // a check for completed raster tasks to ensure this upload is dispatched
  // without too much latency.
  if (bytes_pending_raster)
    ScheduleCheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::OnRasterTaskCompleted(
    scoped_refptr<internal::RasterWorkerPoolTask> task,
    bool was_canceled,
    bool needs_upload) {
  TRACE_EVENT2("cc", "PixelBufferRasterWorkerPool::OnRasterTaskCompleted",
               "was_canceled", was_canceled,
               "needs_upload", needs_upload);

  DCHECK(pixel_buffer_tasks_.find(task.get()) != pixel_buffer_tasks_.end());

  // Balanced with MapPixelBuffer() call in ScheduleMoreTasks().
  resource_provider()->UnmapPixelBuffer(task->resource()->id());

  if (!needs_upload) {
    resource_provider()->ReleasePixelBuffer(task->resource()->id());
    task->DidRun(was_canceled);
    DCHECK(std::find(completed_tasks_.begin(),
                     completed_tasks_.end(),
                     task) == completed_tasks_.end());
    completed_tasks_.push_back(task);
    return;
  }

  resource_provider()->BeginSetPixels(task->resource()->id());
  has_performed_uploads_since_last_flush_ = true;

  bytes_pending_upload_ += task->resource()->bytes();
  tasks_with_pending_upload_.push_back(task);
}

}  // namespace cc

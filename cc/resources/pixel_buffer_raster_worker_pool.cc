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
                                TaskVector* dependencies,
                                uint8_t* buffer,
                                const Reply& reply)
      : internal::WorkerPoolTask(dependencies),
        task_(task),
        buffer_(buffer),
        reply_(reply),
        needs_upload_(false) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
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
    needs_upload_ = task_->RunOnThread(&device, thread_index);
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
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
// For reference, the Nexus10 can upload 1MB in about 2.5ms.
// Assuming a three frame deep pipeline this implies ~20MB.
const size_t kMaxPendingUploadBytes = 20 * 1024 * 1024;
// TODO(epenner): We should remove this upload limit (crbug.com/176197)
const size_t kMaxPendingUploads = 72;
#else
const size_t kMaxPendingUploadBytes = 100 * 1024 * 1024;
const size_t kMaxPendingUploads = 1000;
#endif

const int kCheckForCompletedRasterTasksDelayMs = 6;

}  // namespace

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    ResourceProvider* resource_provider,
    size_t num_threads) : RasterWorkerPool(resource_provider, num_threads),
                          bytes_pending_upload_(0),
                          has_performed_uploads_since_last_flush_(false),
                          check_for_completed_raster_tasks_pending_(false),
                          weak_ptr_factory_(this),
                          schedule_more_tasks_count_(0) {
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK(!check_for_completed_raster_tasks_pending_);
  DCHECK_EQ(0u, pixel_buffer_tasks_.size());
  DCHECK_EQ(0u, tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_tasks_.size());
}

void PixelBufferRasterWorkerPool::Shutdown() {
  RasterWorkerPool::Shutdown();
  CheckForCompletedRasterTasks();
  AbortPendingUploads();
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    internal::WorkerPoolTask* pixel_buffer_task = it->second.get();

    // All inactive tasks needs to be canceled.
    if (!pixel_buffer_task)
      completed_tasks_.push_back(task);
  }
  // Cancel any pending OnRasterFinished callback.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  // Build new pixel buffer task set.
  TaskMap new_pixel_buffer_tasks;
  for (RasterTask::Queue::TaskVector::const_iterator it =
           raster_tasks().begin();
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
    if (!pixel_buffer_task)
      completed_tasks_.push_back(task);
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
  FlushUploads();

  CheckForCompletedUploads();

  while (!completed_tasks_.empty()) {
    internal::RasterWorkerPoolTask* task = completed_tasks_.front().get();
    DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

    pixel_buffer_tasks_.erase(task);

    task->DidComplete();
    task->DispatchCompletionCallback();

    completed_tasks_.pop_front();
  }
}

bool PixelBufferRasterWorkerPool::ForceUploadToComplete(
    const RasterTask& raster_task) {
  for (TaskDeque::iterator it = tasks_with_pending_upload_.begin();
       it != tasks_with_pending_upload_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    if (task == raster_task.internal_.get()) {
      resource_provider()->ForceSetPixelsToComplete(task->resource()->id());
      return true;
    }
  }

  return false;
}

void PixelBufferRasterWorkerPool::FlushUploads() {
  if (!has_performed_uploads_since_last_flush_)
    return;

  resource_provider()->ShallowFlushIfSupported();
  has_performed_uploads_since_last_flush_ = false;
}

void PixelBufferRasterWorkerPool::CheckForCompletedUploads() {
  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_pending_upload_.front().get();
    DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

    // Uploads complete in the order they are issued.
    if (!resource_provider()->DidSetPixelsComplete(task->resource()->id()))
      break;

    // It's now safe to release the pixel buffer and the shared memory.
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    task->DidRun();
    completed_tasks_.push_back(task);

    tasks_with_pending_upload_.pop_front();
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
  FlushUploads();

  CheckForCompletedUploads();

  ScheduleMoreTasks();
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  size_t tasks_with_pending_upload = tasks_with_pending_upload_.size();
  size_t bytes_pending_upload = bytes_pending_upload_;

  internal::WorkerPoolTask::TaskVector tasks;

  for (RasterTask::Queue::TaskVector::const_iterator it =
           raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();

    TaskMap::iterator pixel_buffer_it = pixel_buffer_tasks_.find(task);
    if (pixel_buffer_it == pixel_buffer_tasks_.end())
      continue;

    scoped_refptr<internal::WorkerPoolTask> pixel_buffer_task(
        pixel_buffer_it->second);
    if (pixel_buffer_task.get()) {
      if (!pixel_buffer_task->HasCompleted())
        tasks.push_back(pixel_buffer_task);
      continue;
    }

    // Throttle raster tasks based on number of pending uploads.
    size_t new_tasks_with_pending_upload = tasks_with_pending_upload;
    new_tasks_with_pending_upload += 1;
    if (new_tasks_with_pending_upload > kMaxPendingUploads)
      break;

    // Throttle raster tasks based on bytes of pending uploads.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > kMaxPendingUploadBytes)
      break;

    // Update |tasks_with_pending_upload| and |bytes_pending_upload|
    // now that task has cleared throttling limits.
    tasks_with_pending_upload = new_tasks_with_pending_upload;
    bytes_pending_upload = new_bytes_pending_upload;

    // Request a pixel buffer. This will reserve shared memory.
    resource_provider()->AcquirePixelBuffer(task->resource()->id());

    // MapPixelBuffer() returns NULL if context was lost at the time
    // AcquirePixelBuffer() was called. For simplicity we still post
    // a raster task that is essentially a noop in these situations.
    uint8* buffer = resource_provider()->MapPixelBuffer(
        task->resource()->id());

    // TODO(reveman): Avoid having to make a copy of dependencies.
    internal::WorkerPoolTask::TaskVector dependencies = task->dependencies();
    pixel_buffer_task = make_scoped_refptr(
        new PixelBufferWorkerPoolTaskImpl(
            task,
            &dependencies,
            buffer,
            base::Bind(&PixelBufferRasterWorkerPool::OnRasterTaskCompleted,
                       base::Unretained(this),
                       make_scoped_refptr(task))));

    pixel_buffer_tasks_[task] = pixel_buffer_task;
    tasks.push_back(pixel_buffer_task);
  }

  ++schedule_more_tasks_count_;

  // We need to make sure not to schedule a check for completed raster
  // tasks when |tasks| is empty as that would cause us to never stop
  // checking.
  if (tasks.empty()) {
    ScheduleRasterTasks(RootTask());
    return;
  }

  RootTask root(
      base::Bind(&PixelBufferRasterWorkerPool::RunRasterFinishedTask,
                 base::MessageLoopProxy::current(),
                 base::Bind(&PixelBufferRasterWorkerPool::OnRasterFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            schedule_more_tasks_count_)),
                &tasks);
  ScheduleRasterTasks(root);

  // At least one task that could need an upload is now pending, schedule
  // a check for completed raster tasks to ensure this upload is dispatched
  // without too much latency.
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
    // No upload needed. Dispatch completion callback.
    if (!was_canceled)
      task->DidRun();

    completed_tasks_.push_back(task);
    return;
  }

  resource_provider()->BeginSetPixels(task->resource()->id());
  has_performed_uploads_since_last_flush_ = true;

  bytes_pending_upload_ += task->resource()->bytes();
  tasks_with_pending_upload_.push_back(task);
}

void PixelBufferRasterWorkerPool::AbortPendingUploads() {
  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_pending_upload_.front().get();
    DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

    resource_provider()->AbortSetPixels(task->resource()->id());
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    // Need to run the reply callback even though task was aborted.
    completed_tasks_.push_back(task);

    tasks_with_pending_upload_.pop_front();
  }
}

void PixelBufferRasterWorkerPool::OnRasterFinished(
    int64 schedule_more_tasks_count) {
  TRACE_EVENT1("cc",
               "PixelBufferRasterWorkerPool::OnRasterFinishedTasks",
               "schedule_more_tasks_count", schedule_more_tasks_count);
  DCHECK_GE(schedule_more_tasks_count_, schedule_more_tasks_count);
  // Call CheckForCompletedTasks() when we've finished running all raster
  // tasks needed since last time ScheduleMoreTasks() was called. This
  // reduces latency when processing only a small number of raster tasks.
  if (schedule_more_tasks_count_ == schedule_more_tasks_count)
    CheckForCompletedRasterTasks();
}

// static
void PixelBufferRasterWorkerPool::RunRasterFinishedTask(
    scoped_refptr<base::MessageLoopProxy> origin_loop,
    const base::Closure& on_raster_finished_callback) {
  TRACE_EVENT0("cc", "RasterWorkerPool::RunRasterFinishedTask");
  origin_loop->PostTask(FROM_HERE, on_raster_finished_callback);
}

}  // namespace cc

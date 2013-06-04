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

}  // namespace

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    ResourceProvider* resource_provider,
    size_t num_threads) : RasterWorkerPool(resource_provider, num_threads),
                          bytes_pending_upload_(0),
                          has_performed_uploads_since_last_flush_(false) {
  // TODO(reveman): Remove WorkerPool client interface.
  WorkerPool::SetClient(this);
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK_EQ(0u, pixel_buffer_tasks_.size());
  DCHECK_EQ(0u, tasks_with_pending_upload_.size());
}

void PixelBufferRasterWorkerPool::Shutdown() {
  RasterWorkerPool::Shutdown();
  AbortPendingUploads();
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    DCHECK(!it->second);

    // Everything else has been canceled.
    DidFinishRasterTask(task);
  }
  pixel_buffer_tasks_.clear();
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  // Build new pixel buffer task set.
  TaskMap new_pixel_buffer_tasks;
  for (RasterTask::Queue::TaskVector::iterator it = raster_tasks_.begin();
       it != raster_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = *it;
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

  // Transfer active pixel buffer tasks to |new_pixel_buffer_tasks|
  // and cancel any remaining tasks.
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;

    // Move task to |new_pixel_buffer_tasks|
    if (it->second) {
      new_pixel_buffer_tasks[task] = it->second;
      continue;
    }

    // Everything else can be canceled.
    DidFinishRasterTask(task);
  }

  pixel_buffer_tasks_.swap(new_pixel_buffer_tasks);

  ScheduleMoreTasks();
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  WorkerPool::CheckForCompletedTasks();

  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task = tasks_with_pending_upload_.front();

    // Uploads complete in the order they are issued.
    if (!resource_provider_->DidSetPixelsComplete(task->resource()->id()))
      break;

    // It's now safe to release the pixel buffer and the shared memory.
    resource_provider_->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    task->DidRun();
    DidFinishRasterTask(task);
    pixel_buffer_tasks_.erase(task);

    tasks_with_pending_upload_.pop_front();
  }

  ScheduleMoreTasks();
}

bool PixelBufferRasterWorkerPool::ForceUploadToComplete(
    const RasterTask& raster_task) {
  for (TaskDeque::iterator it = tasks_with_pending_upload_.begin();
       it != tasks_with_pending_upload_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = *it;
    if (task == raster_task.internal_) {
      resource_provider_->ForceSetPixelsToComplete(task->resource()->id());
      return true;
    }
  }

  return false;
}

void PixelBufferRasterWorkerPool::
    DidFinishDispatchingWorkerPoolCompletionCallbacks() {
  // If a flush is needed, do it now before starting to dispatch more tasks.
  if (has_performed_uploads_since_last_flush_) {
    resource_provider_->ShallowFlushIfSupported();
    has_performed_uploads_since_last_flush_ = false;
  }
}

bool PixelBufferRasterWorkerPool::CanScheduleRasterTask(
    internal::RasterWorkerPoolTask* task) {
  if (tasks_with_pending_upload_.size() >= kMaxPendingUploads)
    return false;
  size_t new_bytes_pending = bytes_pending_upload_;
  new_bytes_pending += task->resource()->bytes();
  return new_bytes_pending <= kMaxPendingUploadBytes;
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  internal::WorkerPoolTask::TaskVector raster_tasks;

  for (RasterTask::Queue::TaskVector::iterator it = raster_tasks_.begin();
       it != raster_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = *it;

    TaskMap::iterator pixel_buffer_it = pixel_buffer_tasks_.find(task);
    if (pixel_buffer_it == pixel_buffer_tasks_.end())
      continue;

    scoped_refptr<internal::WorkerPoolTask> pixel_buffer_task(
        pixel_buffer_it->second);
    if (pixel_buffer_task) {
      if (!pixel_buffer_task->HasCompleted())
        raster_tasks.push_back(pixel_buffer_task);
      continue;
    }

    if (!CanScheduleRasterTask(task))
      break;

    // Request a pixel buffer. This will reserve shared memory.
    resource_provider_->AcquirePixelBuffer(task->resource()->id());

    // MapPixelBuffer() returns NULL if context was lost at the time
    // AcquirePixelBuffer() was called. For simplicity we still post
    // a raster task that is essentially a noop in these situations.
    uint8* buffer = resource_provider_->MapPixelBuffer(task->resource()->id());

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
    raster_tasks.push_back(pixel_buffer_task);
  }

  ScheduleRasterTasks(&raster_tasks);
}

void PixelBufferRasterWorkerPool::OnRasterTaskCompleted(
    scoped_refptr<internal::RasterWorkerPoolTask> task,
    bool was_canceled,
    bool needs_upload) {
  TRACE_EVENT2("cc", "PixelBufferRasterWorkerPool::OnRasterTaskCompleted",
               "was_canceled", was_canceled,
               "needs_upload", needs_upload);

  DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

  // Balanced with MapPixelBuffer() call in ScheduleMoreTasks().
  resource_provider_->UnmapPixelBuffer(task->resource()->id());

  if (!needs_upload) {
    resource_provider_->ReleasePixelBuffer(task->resource()->id());
    // No upload needed. Dispatch completion callback.
    if (!was_canceled)
      task->DidRun();

    DidFinishRasterTask(task);
    pixel_buffer_tasks_.erase(task);
    return;
  }

  resource_provider_->BeginSetPixels(task->resource()->id());
  has_performed_uploads_since_last_flush_ = true;

  bytes_pending_upload_ += task->resource()->bytes();
  tasks_with_pending_upload_.push_back(task);
}

void PixelBufferRasterWorkerPool::AbortPendingUploads() {
  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task = tasks_with_pending_upload_.front();

    resource_provider_->AbortSetPixels(task->resource()->id());
    resource_provider_->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    // Need to run the reply callback even though task was aborted.
    DidFinishRasterTask(task);
    pixel_buffer_tasks_.erase(task);

    tasks_with_pending_upload_.pop_front();
  }
}

void PixelBufferRasterWorkerPool::DidFinishRasterTask(
    internal::RasterWorkerPoolTask* task) {
  task->DidComplete();
  task->DispatchCompletionCallback();
}

}  // namespace cc

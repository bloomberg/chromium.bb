// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/resources/resource.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

namespace {

class ImageWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(bool was_canceled)> Reply;

  ImageWorkerPoolTaskImpl(internal::RasterWorkerPoolTask* task,
                          uint8_t* buffer,
                          int stride,
                          const Reply& reply)
      : task_(task),
        buffer_(buffer),
        stride_(stride),
        reply_(reply) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    if (!buffer_)
      return;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     task_->resource()->size().width(),
                     task_->resource()->size().height(),
                     stride_);
    bitmap.setPixels(buffer_);
    SkDevice device(bitmap);
    task_->RunOnWorkerThread(&device, thread_index);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 private:
  virtual ~ImageWorkerPoolTaskImpl() {}

  scoped_refptr<internal::RasterWorkerPoolTask> task_;
  uint8_t* buffer_;
  int stride_;
  const Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageWorkerPoolTaskImpl);
};

}  // namespace

ImageRasterWorkerPool::ImageRasterWorkerPool(
    ResourceProvider* resource_provider, size_t num_threads)
    : RasterWorkerPool(resource_provider, num_threads) {
}

ImageRasterWorkerPool::~ImageRasterWorkerPool() {
  DCHECK_EQ(0u, image_tasks_.size());
}

void ImageRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  RasterTaskGraph graph;
  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();

    TaskMap::iterator image_it = image_tasks_.find(task);
    if (image_it != image_tasks_.end()) {
      internal::WorkerPoolTask* image_task = image_it->second.get();
      graph.InsertRasterTask(image_task, task->dependencies());
      continue;
    }

    // Acquire image for resource.
    resource_provider()->AcquireImage(task->resource()->id());

    // Map image for raster.
    uint8* buffer = resource_provider()->MapImage(task->resource()->id());
    int stride = resource_provider()->GetImageStride(task->resource()->id());

    scoped_refptr<internal::WorkerPoolTask> new_image_task(
        new ImageWorkerPoolTaskImpl(
            task,
            buffer,
            stride,
            base::Bind(&ImageRasterWorkerPool::OnRasterTaskCompleted,
                       base::Unretained(this),
                       make_scoped_refptr(task))));
    image_tasks_[task] = new_image_task;
    graph.InsertRasterTask(new_image_task.get(), task->dependencies());
  }

  SetRasterTaskGraph(&graph);
}

void ImageRasterWorkerPool::OnRasterTaskCompleted(
    scoped_refptr<internal::RasterWorkerPoolTask> task,
    bool was_canceled) {
  TRACE_EVENT1("cc", "ImageRasterWorkerPool::OnRasterTaskCompleted",
               "was_canceled", was_canceled);

  DCHECK(image_tasks_.find(task.get()) != image_tasks_.end());

  // Balanced with MapImage() call in ScheduleTasks().
  resource_provider()->UnmapImage(task->resource()->id());

  // Bind image to resource.
  resource_provider()->BindImage(task->resource()->id());

  task->DidRun(was_canceled);
  task->WillComplete();
  task->CompleteOnOriginThread();
  task->DidComplete();

  image_tasks_.erase(task.get());
}

}  // namespace cc

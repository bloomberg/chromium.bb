// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "cc/resources/picture_pile_impl.h"

namespace cc {

namespace {

class RasterWorkerPoolContainerTaskImpl : public internal::WorkerPoolTask {
 public:
  RasterWorkerPoolContainerTaskImpl(
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {}
  virtual void DispatchCompletionCallback() OVERRIDE {}

 private:
  virtual ~RasterWorkerPoolContainerTaskImpl() {}
};

class RasterWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(const base::Closure& callback,
                           const RasterWorkerPool::Task::Reply& reply)
      : callback_(callback),
        reply_(reply) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    callback_.Run();
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 private:
  virtual ~RasterWorkerPoolTaskImpl() {}

  const base::Closure callback_;
  const RasterWorkerPool::Task::Reply reply_;
};

class RasterWorkerPoolPictureTaskImpl : public internal::WorkerPoolTask {
 public:
  RasterWorkerPoolPictureTaskImpl(
      PicturePileImpl* picture_pile,
      const RasterWorkerPool::PictureTask::Callback& callback,
      const RasterWorkerPool::Task::Reply& reply,
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        picture_pile_(picture_pile),
        callback_(callback),
        reply_(reply) {
    DCHECK(picture_pile_);
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    callback_.Run(picture_pile_->GetCloneForDrawingOnThread(thread_index));
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 private:
  virtual ~RasterWorkerPoolPictureTaskImpl() {}

  scoped_refptr<PicturePileImpl> picture_pile_;
  const RasterWorkerPool::PictureTask::Callback callback_;
  const RasterWorkerPool::Task::Reply reply_;
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

const int kCheckForCompletedTasksDelayMs = 6;

}  // namespace

RasterWorkerPool::Task::Queue::Queue() {
}

RasterWorkerPool::Task::Queue::~Queue() {
}

void RasterWorkerPool::Task::Queue::Append(const Task& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::Task::Task() {
}

RasterWorkerPool::Task::Task(const base::Closure& callback,
                             const Reply& reply)
    : internal_(new RasterWorkerPoolTaskImpl(callback, reply)) {
}

RasterWorkerPool::Task::Task(Queue* dependencies)
    : internal_(new RasterWorkerPoolContainerTaskImpl(&dependencies->tasks_)) {
}

RasterWorkerPool::Task::Task(scoped_refptr<internal::WorkerPoolTask> internal)
    : internal_(internal) {
}

RasterWorkerPool::Task::~Task() {
}

void RasterWorkerPool::Task::Reset() {
  internal_ = NULL;
}

RasterWorkerPool::PictureTask::PictureTask(PicturePileImpl* picture_pile,
                                           const Callback& callback,
                                           const Reply& reply,
                                           Task::Queue* dependencies)
    : RasterWorkerPool::Task(
        new RasterWorkerPoolPictureTaskImpl(picture_pile,
                                            callback,
                                            reply,
                                            &dependencies->tasks_)) {
}

RasterWorkerPool::RasterWorkerPool(size_t num_threads) : WorkerPool(
    num_threads,
    base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs),
    kWorkerThreadNamePrefix) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::Shutdown() {
  // Cancel all previously scheduled tasks.
  WorkerPool::ScheduleTasks(NULL);

  WorkerPool::Shutdown();
}

void RasterWorkerPool::ScheduleTasks(Task* task) {
  WorkerPool::ScheduleTasks(task ? task->internal_ : NULL);
}

}  // namespace cc

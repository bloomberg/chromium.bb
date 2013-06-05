// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "cc/resources/picture_pile_impl.h"

namespace cc {

namespace {

void Noop() {}

class WorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  WorkerPoolTaskImpl(const base::Closure& callback,
                     const base::Closure& reply)
      : callback_(callback),
        reply_(reply) {
  }
  explicit WorkerPoolTaskImpl(
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        callback_(base::Bind(&Noop)),
        reply_(base::Bind(&Noop)) {
  }
  WorkerPoolTaskImpl(const base::Closure& callback,
                     internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        callback_(callback),
        reply_(base::Bind(&Noop)) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    callback_.Run();
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run();
  }

 private:
  virtual ~WorkerPoolTaskImpl() {}

  const base::Closure callback_;
  const base::Closure reply_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPoolTaskImpl);
};

class RasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(
      PicturePileImpl* picture_pile,
      const Resource* resource,
      const RasterWorkerPool::RasterTask::Callback& callback,
      const RasterWorkerPool::RasterTask::Reply& reply,
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        picture_pile_(picture_pile),
        callback_(callback),
        reply_(reply) {
  }

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnThread(SkDevice* device, unsigned thread_index) OVERRIDE {
    return callback_.Run(
        device, picture_pile_->GetCloneForDrawingOnThread(thread_index));
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 private:
  virtual ~RasterWorkerPoolTaskImpl() {}

  scoped_refptr<PicturePileImpl> picture_pile_;
  const RasterWorkerPool::RasterTask::Callback callback_;
  const RasterWorkerPool::RasterTask::Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPoolTaskImpl);
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

const int kCheckForCompletedTasksDelayMs = 6;

}  // namespace

namespace internal {

RasterWorkerPoolTask::RasterWorkerPoolTask(
    const Resource* resource,
    WorkerPoolTask::TaskVector* dependencies)
    : did_run_(false),
      did_complete_(false),
      resource_(resource) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {
}

void RasterWorkerPoolTask::DidRun() {
  DCHECK(!did_run_);
  did_run_ = true;
}

bool RasterWorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

void RasterWorkerPoolTask::DidComplete() {
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool RasterWorkerPoolTask::HasCompleted() const {
  return did_complete_;
}

}  // namespace internal

RasterWorkerPool::Task::Set::Set() {
}

RasterWorkerPool::Task::Set::~Set() {
}

void RasterWorkerPool::Task::Set::Insert(const Task& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::Task::Task() {
}

RasterWorkerPool::Task::Task(const base::Closure& callback,
                             const base::Closure& reply)
    : internal_(new WorkerPoolTaskImpl(callback, reply)) {
}

RasterWorkerPool::Task::~Task() {
}

void RasterWorkerPool::Task::Reset() {
  internal_ = NULL;
}

RasterWorkerPool::RasterTask::Queue::Queue() {
}

RasterWorkerPool::RasterTask::Queue::~Queue() {
}

void RasterWorkerPool::RasterTask::Queue::Append(const RasterTask& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::RasterTask::RasterTask() {
}

RasterWorkerPool::RasterTask::RasterTask(PicturePileImpl* picture_pile,
                                         const Resource* resource,
                                         const Callback& callback,
                                         const Reply& reply,
                                         Task::Set* dependencies)
    : internal_(new RasterWorkerPoolTaskImpl(picture_pile,
                                             resource,
                                             callback,
                                             reply,
                                             &dependencies->tasks_)) {
}

void RasterWorkerPool::RasterTask::Reset() {
  internal_ = NULL;
}

RasterWorkerPool::RasterTask::~RasterTask() {
}

RasterWorkerPool::RootTask::RootTask() {
}

RasterWorkerPool::RootTask::RootTask(
    internal::WorkerPoolTask::TaskVector* dependencies)
    : internal_(new WorkerPoolTaskImpl(dependencies)) {
}

RasterWorkerPool::RootTask::RootTask(
    const base::Closure& callback,
    internal::WorkerPoolTask::TaskVector* dependencies)
    : internal_(new WorkerPoolTaskImpl(callback, dependencies)) {
}

RasterWorkerPool::RootTask::~RootTask() {
}

RasterWorkerPool::RasterWorkerPool(ResourceProvider* resource_provider,
                                   size_t num_threads)
    : WorkerPool(
        num_threads,
        base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs),
        kWorkerThreadNamePrefix),
      resource_provider_(resource_provider) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::Shutdown() {
  raster_tasks_.clear();
  WorkerPool::Shutdown();
}

bool RasterWorkerPool::ForceUploadToComplete(const RasterTask& raster_task) {
  return false;
}

void RasterWorkerPool::SetRasterTasks(RasterTask::Queue* queue) {
  raster_tasks_.swap(queue->tasks_);
}

void RasterWorkerPool::ScheduleRasterTasks(const RootTask& root) {
  WorkerPool::ScheduleTasks(root.internal_);
}

}  // namespace cc

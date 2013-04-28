// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "cc/resources/picture_pile_impl.h"

namespace cc {

namespace {

class RasterWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(PicturePileImpl* picture_pile,
                           const RasterWorkerPool::RasterCallback& task,
                           const base::Closure& reply)
      : internal::WorkerPoolTask(reply),
        picture_pile_(picture_pile),
        task_(task) {
    DCHECK(picture_pile_);
  }

  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    task_.Run(picture_pile_->GetCloneForDrawingOnThread(thread_index));
  }

 private:
  scoped_refptr<PicturePileImpl> picture_pile_;
  RasterWorkerPool::RasterCallback task_;
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

const int kCheckForCompletedTasksDelayMs = 6;

}  // namespace

RasterWorkerPool::RasterWorkerPool(
    WorkerPoolClient* client, size_t num_threads) : WorkerPool(
        client,
        num_threads,
        base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs),
        kWorkerThreadNamePrefix) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                                              const RasterCallback& task,
                                              const base::Closure& reply) {
  PostTask(make_scoped_ptr(new RasterWorkerPoolTaskImpl(
                               picture_pile,
                               task,
                               reply)).PassAs<internal::WorkerPoolTask>());
}

}  // namespace cc

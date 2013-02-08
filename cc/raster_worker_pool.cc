// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster_worker_pool.h"

#include "cc/picture_pile_impl.h"

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

  virtual void Run(RenderingStats* rendering_stats) OVERRIDE {
    task_.Run(picture_pile_.get(), rendering_stats);
  }

 private:
  scoped_refptr<PicturePileImpl> picture_pile_;
  RasterWorkerPool::RasterCallback task_;
};

}  // namespace

RasterWorkerPool::RasterWorkerPool(size_t num_threads)
    : WorkerPool(num_threads) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                                              const RasterCallback& task,
                                              const base::Closure& reply) {
  Worker* worker = GetWorkerForNextTask();

  scoped_refptr<PicturePileImpl> picture_pile_clone =
      picture_pile->GetCloneForDrawingOnThread(worker);

  worker->PostTask(
      make_scoped_ptr(new RasterWorkerPoolTaskImpl(
                          picture_pile_clone.get(),
                          task,
                          reply)).PassAs<internal::WorkerPoolTask>());
}

}  // namespace cc

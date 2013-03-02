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

  virtual void WillRunOnThread(unsigned thread_index) OVERRIDE {
    picture_pile_ = picture_pile_->GetCloneForDrawingOnThread(thread_index);
  }

  virtual void Run(RenderingStats* rendering_stats) OVERRIDE {
    task_.Run(picture_pile_.get(), rendering_stats);
    base::subtle::Release_Store(&completed_, 1);
  }

 private:
  scoped_refptr<PicturePileImpl> picture_pile_;
  RasterWorkerPool::RasterCallback task_;
};

}  // namespace

RasterWorkerPool::RasterWorkerPool(
    WorkerPoolClient* client, size_t num_threads)
    : WorkerPool(client, num_threads) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                                              bool is_cheap,
                                              const RasterCallback& task,
                                              const base::Closure& reply) {
  PostTask(
      make_scoped_ptr(new RasterWorkerPoolTaskImpl(
                          picture_pile,
                          task,
                          reply)).PassAs<internal::WorkerPoolTask>(),
                      is_cheap);
}

}  // namespace cc

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_WORKER_POOL_H_
#define CC_RASTER_WORKER_POOL_H_

#include <string>

#include "cc/worker_pool.h"

namespace cc {
class PicturePileImpl;

// A worker thread pool that runs raster tasks.
class RasterWorkerPool : public WorkerPool {
 public:
  typedef base::Callback<void(PicturePileImpl*, RenderingStats*)>
      RasterCallback;

  virtual ~RasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      size_t num_threads, bool record_rendering_stats) {
    return make_scoped_ptr(
        new RasterWorkerPool(num_threads, record_rendering_stats));
  }

  void PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                              const RasterCallback& task,
                              const base::Closure& reply);

 private:
  RasterWorkerPool(size_t num_threads, bool record_rendering_stats);

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RASTER_WORKER_POOL_H_

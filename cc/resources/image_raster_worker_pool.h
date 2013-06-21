// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT ImageRasterWorkerPool : public RasterWorkerPool {
 public:
  virtual ~ImageRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      ResourceProvider* resource_provider, size_t num_threads) {
    return make_scoped_ptr<RasterWorkerPool>(
        new ImageRasterWorkerPool(resource_provider, num_threads));
  }

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE;

 private:
  ImageRasterWorkerPool(ResourceProvider* resource_provider,
                        size_t num_threads);

  void OnRasterTaskCompleted(
      scoped_refptr<internal::RasterWorkerPoolTask> task, bool was_canceled);

  TaskMap image_tasks_;

  DISALLOW_COPY_AND_ASSIGN(ImageRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_

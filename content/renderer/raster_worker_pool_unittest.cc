// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/sequenced_task_runner_test_template.h"
#include "base/test/task_runner_test_template.h"
#include "base/threading/simple_thread.h"
#include "content/renderer/raster_worker_pool.h"

namespace base {
namespace {

// Number of threads spawned in tests.
const int kNumThreads = 4;

class RasterWorkerPoolTestDelegate {
 public:
  RasterWorkerPoolTestDelegate()
      : raster_worker_pool_(new content::RasterWorkerPool()) {}

  void StartTaskRunner() {
    raster_worker_pool_->Start(kNumThreads, SimpleThread::Options());
  }

  scoped_refptr<content::RasterWorkerPool> GetTaskRunner() {
    return raster_worker_pool_;
  }

  void StopTaskRunner() {
    raster_worker_pool_->GetTaskGraphRunner()->FlushForTesting();
  }

  ~RasterWorkerPoolTestDelegate() { raster_worker_pool_->Shutdown(); }

 private:
  scoped_refptr<content::RasterWorkerPool> raster_worker_pool_;
};

INSTANTIATE_TYPED_TEST_CASE_P(RasterWorkerPool,
                              TaskRunnerTest,
                              RasterWorkerPoolTestDelegate);

class RasterWorkerPoolSequencedTestDelegate {
 public:
  RasterWorkerPoolSequencedTestDelegate()
      : raster_worker_pool_(new content::RasterWorkerPool()) {}

  void StartTaskRunner() {
    raster_worker_pool_->Start(kNumThreads, SimpleThread::Options());
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return raster_worker_pool_->CreateSequencedTaskRunner();
  }

  void StopTaskRunner() {
    raster_worker_pool_->GetTaskGraphRunner()->FlushForTesting();
  }

  ~RasterWorkerPoolSequencedTestDelegate() { raster_worker_pool_->Shutdown(); }

 private:
  scoped_refptr<content::RasterWorkerPool> raster_worker_pool_;
};

INSTANTIATE_TYPED_TEST_CASE_P(RasterWorkerPool,
                              SequencedTaskRunnerTest,
                              RasterWorkerPoolSequencedTestDelegate);

}  // namespace
}  // namespace base

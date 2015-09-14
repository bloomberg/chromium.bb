// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/raster_thread_helper.h"

#include "base/logging.h"
#include "base/threading/simple_thread.h"
#include "cc/raster/task_graph_runner.h"

namespace gles2 {

class RasterThreadHelper::RasterThread : public base::SimpleThread {
 public:
  RasterThread(cc::TaskGraphRunner* task_graph_runner)
      : base::SimpleThread("CompositorTileWorker1"),
        task_graph_runner_(task_graph_runner) {}

  // Overridden from base::SimpleThread:
  void Run() override { task_graph_runner_->Run(); }

 private:
  cc::TaskGraphRunner* task_graph_runner_;

  DISALLOW_COPY_AND_ASSIGN(RasterThread);
};

RasterThreadHelper::RasterThreadHelper()
    : task_graph_runner_(new cc::TaskGraphRunner),
      raster_thread_(new RasterThread(task_graph_runner_.get())) {
  raster_thread_->Start();
}

RasterThreadHelper::~RasterThreadHelper() {
  task_graph_runner_->Shutdown();
  raster_thread_->Join();
}

}  // namespace gles2

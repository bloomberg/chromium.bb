// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/blimp_task_graph_runner.h"

namespace blimp {

BlimpTaskGraphRunner::BlimpTaskGraphRunner()
    : worker_thread_(
          this,
          "CompositorTileWorker1",
          base::SimpleThread::Options(base::ThreadPriority::BACKGROUND)) {
  worker_thread_.Start();
}

BlimpTaskGraphRunner::~BlimpTaskGraphRunner() {
  Shutdown();
  worker_thread_.Join();
}

void BlimpTaskGraphRunner::Run() {
  cc::TaskGraphRunner::Run();
}

}  // namespace blimp

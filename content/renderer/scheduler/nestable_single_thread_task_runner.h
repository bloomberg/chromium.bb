// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_
#define CONTENT_RENDERER_SCHEDULER_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"

namespace content {

// A single thread task runner which exposes whether it is running nested.
class CONTENT_EXPORT NestableSingleThreadTaskRunner
    : public base::SingleThreadTaskRunner {
 public:
  NestableSingleThreadTaskRunner() {}

  // Returns true if the task runner is nested (i.e., running a run loop within
  // a nested task).
  virtual bool IsNested() const = 0;

 protected:
  ~NestableSingleThreadTaskRunner() override {}

  DISALLOW_COPY_AND_ASSIGN(NestableSingleThreadTaskRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_

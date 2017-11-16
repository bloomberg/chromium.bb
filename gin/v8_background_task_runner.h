// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_BACKGROUND_TASK_RUNNER_H
#define GIN_V8_BACKGROUND_TASK_RUNNER_H

#include "v8/include/v8-platform.h"

namespace gin {

class V8BackgroundTaskRunner : public v8::TaskRunner {
 public:
  // Returns the number of available background threads, which is always >= 1.
  static size_t NumberOfAvailableBackgroundThreads();

  // v8::Platform implementation.
  void PostTask(std::unique_ptr<v8::Task> task) override;

  void PostDelayedTask(std::unique_ptr<v8::Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override;

  bool IdleTasksEnabled() override;
};

}  // namespace gin

#endif  // GIN_V8_BACKGROUND_TASK_RUNNER_H

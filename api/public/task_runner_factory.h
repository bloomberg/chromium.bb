// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_TASK_RUNNER_FACTORY_H_
#define API_PUBLIC_TASK_RUNNER_FACTORY_H_

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <thread>  // NOLINT
#include <utility>

#include "api/public/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

class TaskRunnerFactory {
 public:
  // Creates a instantiated TaskRunner
  static std::unique_ptr<TaskRunner> Create(
      platform::ClockNowFunctionPtr now_function);
};
}  // namespace platform
}  // namespace openscreen

#endif  // API_PUBLIC_TASK_RUNNER_FACTORY_H_

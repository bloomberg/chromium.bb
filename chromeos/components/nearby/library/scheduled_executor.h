// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SCHEDULED_EXECUTOR_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SCHEDULED_EXECUTOR_H_

#include <cstdint>
#include <memory>

#include "chromeos/components/nearby/library/cancelable.h"
#include "chromeos/components/nearby/library/executor.h"
// TODO(kyleqian): Use Ptr once the Nearby library is merged in.
// #include "location/nearby/cpp/platform/ptr.h"
#include "chromeos/components/nearby/library/runnable.h"

namespace location {
namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public Executor {
 public:
  ~ScheduledExecutor() override {}

  virtual std::shared_ptr<Cancelable> schedule(
      std::shared_ptr<Runnable> runnable,
      std::int64_t delay_millis) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SCHEDULED_EXECUTOR_H_

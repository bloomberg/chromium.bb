// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/lazy_now.h"

#include "components/scheduler/base/task_queue_manager.h"

namespace scheduler {
namespace internal {

base::TimeTicks LazyNow::Now() {
  if (now_.is_null())
    now_ = task_queue_manager_->Now();
  return now_;
}

}  // namespace internal
}  // namespace scheduler

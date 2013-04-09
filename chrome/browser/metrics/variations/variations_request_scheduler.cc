// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_request_scheduler.h"

namespace chrome_variations {

namespace {

// Time between seed fetches, in hours.
const int kSeedFetchPeriodHours = 5;

}  // namespace

VariationsRequestScheduler::VariationsRequestScheduler(
    const base::Closure& task) : task_(task) {
}

VariationsRequestScheduler::~VariationsRequestScheduler() {
}

void VariationsRequestScheduler::Start() {
  // Call the task and repeat it periodically.
  task_.Run();
  timer_.Start(FROM_HERE, base::TimeDelta::FromHours(kSeedFetchPeriodHours),
               task_);
}

void VariationsRequestScheduler::Reset() {
  if (timer_.IsRunning())
    timer_.Reset();
}

base::Closure VariationsRequestScheduler::task() const {
  return task_;
}

#if !defined(OS_ANDROID)
// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::Closure& task,
    PrefService* local_state) {
  return new VariationsRequestScheduler(task);
}
#endif  // !defined(OS_ANDROID)

}  // namespace chrome_variations

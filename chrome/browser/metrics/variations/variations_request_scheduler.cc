// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_request_scheduler.h"

#include "base/message_loop.h"
#include "chrome/browser/metrics/variations/variations_service.h"

namespace chrome_variations {

namespace {

// Time between seed fetches, in hours.
const int kSeedFetchPeriodHours = 5;

}  // namespace

VariationsRequestScheduler::VariationsRequestScheduler(
    const base::Closure& task) : task_(task) {
  // Call the task immediately.
  task.Run();

  // Repeat this periodically.
  timer_.Start(FROM_HERE, base::TimeDelta::FromHours(kSeedFetchPeriodHours),
               task);
}

VariationsRequestScheduler::~VariationsRequestScheduler() {
}

void VariationsRequestScheduler::Reset() {
  if (timer_.IsRunning())
    timer_.Reset();
}

}  // namespace chrome_variations

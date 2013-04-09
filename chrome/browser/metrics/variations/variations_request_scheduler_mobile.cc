// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_request_scheduler_mobile.h"

#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace chrome_variations {

namespace {

// Time between seed fetches, in hours.
const int kSeedFetchPeriodHours = 5;

}  // namespace

VariationsRequestSchedulerMobile::VariationsRequestSchedulerMobile(
    const base::Closure& task,
    PrefService* local_state) :
  VariationsRequestScheduler(task), local_state_(local_state) {
}

VariationsRequestSchedulerMobile::~VariationsRequestSchedulerMobile() {
}

void VariationsRequestSchedulerMobile::Start() {
  // Check the time of the last request. If it has been longer than the fetch
  // period, run the task. Otherwise, do nothing. Note that no future requests
  // are scheduled since it is unlikely that the mobile process would live long
  // enough for the timer to fire.
  const base::Time last_fetch_time = base::Time::FromInternalValue(
      local_state_->GetInt64(prefs::kVariationsLastFetchTime));
  if (base::Time::Now() >
      last_fetch_time + base::TimeDelta::FromHours(kSeedFetchPeriodHours)) {
    task().Run();
  }
}

void VariationsRequestSchedulerMobile::Reset() {
}

// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::Closure& task,
    PrefService* local_state) {
  return new VariationsRequestSchedulerMobile(task, local_state);
}

}  // namespace chrome_variations

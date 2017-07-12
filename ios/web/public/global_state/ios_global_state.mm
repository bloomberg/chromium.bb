// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/global_state/ios_global_state.h"

#include "base/memory/ptr_util.h"
#include "base/task_scheduler/initialization_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

base::TaskScheduler::InitParams GetDefaultTaskSchedulerInitParams() {
  using StandbyThreadPolicy =
      base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
  return base::TaskScheduler::InitParams(
      base::SchedulerWorkerPoolParams(
          StandbyThreadPolicy::ONE,
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          StandbyThreadPolicy::ONE,
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          StandbyThreadPolicy::ONE,
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          StandbyThreadPolicy::ONE,
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(60)));
}

}  // namespace

namespace ios_global_state {

void Create() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // Use an empty string as TaskScheduler name to match the suffix of browser
    // process TaskScheduler histograms.
    base::TaskScheduler::Create("");
  });
}

void StartTaskScheduler(base::TaskScheduler::InitParams* params) {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    auto init_params = params ? *params : GetDefaultTaskSchedulerInitParams();
    base::TaskScheduler::GetInstance()->Start(init_params);
  });
}

}  // namespace ios_global_state

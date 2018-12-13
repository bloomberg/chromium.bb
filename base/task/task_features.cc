// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/feature_list.h"

namespace base {

const Feature kAllTasksUserBlocking{"AllTasksUserBlocking",
                                    FEATURE_DISABLED_BY_DEFAULT};

const Feature kMergeBlockingNonBlockingPools = {
    "MergeBlockingNonBlockingPools", base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kNoDetachBelowInitialCapacity = {
    "NoDetachBelowInitialCapacity", base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kMayBlockTimings = {"MayBlockTimings",
                                  FEATURE_DISABLED_BY_DEFAULT};

const FeatureParam<int> kMayBlockThresholdMicrosecondsParam = {
    &kMayBlockTimings, "MayBlockThresholdMicroseconds", 10000};

const FeatureParam<int> kBlockedWorkersPollMicrosecondsParam = {
    &kMayBlockTimings, "BlockedWorkersPollMicroseconds", 50000};

}  // namespace base

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_sync_parameters.h"

namespace content {

namespace {
const int kMaxSyncAttempts = 3;
const int kInitialRetryDelayMins = 5;
const int kRetryDelayFactor = 3;
const int64_t kMinSyncRecoveryTimeMs = 1000 * 60 * 6;  // 6 minutes
}

BackgroundSyncParameters::BackgroundSyncParameters()
    : disable(false),
      max_sync_attempts(kMaxSyncAttempts),
      initial_retry_delay(base::TimeDelta::FromMinutes(kInitialRetryDelayMins)),
      retry_delay_factor(kRetryDelayFactor),
      min_sync_recovery_time(
          base::TimeDelta::FromMilliseconds(kMinSyncRecoveryTimeMs)) {}

bool BackgroundSyncParameters::operator==(
    const BackgroundSyncParameters& other) const {
  return disable == other.disable &&
         max_sync_attempts == other.max_sync_attempts &&
         initial_retry_delay == other.initial_retry_delay &&
         retry_delay_factor == other.retry_delay_factor &&
         min_sync_recovery_time == other.min_sync_recovery_time;
}

}  // namespace content

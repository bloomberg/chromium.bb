// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"

namespace content {

void BackgroundSyncMetrics::RecordEventResult(SyncPeriodicity periodicity,
                                              bool success) {
  switch (periodicity) {
    case SYNC_ONE_SHOT:
      UMA_HISTOGRAM_BOOLEAN("BackgroundSync.Event.OneShotResult", success);
      return;
    case SYNC_PERIODIC:
      UMA_HISTOGRAM_BOOLEAN("BackgroundSync.Event.PeriodicResult", success);
      return;
  }
  NOTREACHED();
}

void BackgroundSyncMetrics::RecordBatchSyncEventComplete(
    const base::TimeDelta& time,
    int number_of_batched_sync_events) {
  // The total batch handling time should be under 5 minutes; we'll record up to
  // 6 minutes, to be safe.
  UMA_HISTOGRAM_CUSTOM_TIMES("BackgroundSync.Event.Time", time,
                             base::TimeDelta::FromMilliseconds(10),
                             base::TimeDelta::FromMinutes(6), 50);
  UMA_HISTOGRAM_COUNTS_100("BackgroundSync.Event.BatchSize",
                           number_of_batched_sync_events);
}

void BackgroundSyncMetrics::CountRegister(
    SyncPeriodicity periodicity,
    RegistrationCouldFire registration_could_fire,
    RegistrationIsDuplicate registration_is_duplicate,
    BackgroundSyncStatus result) {
  switch (periodicity) {
    case SYNC_ONE_SHOT:
      UMA_HISTOGRAM_ENUMERATION("BackgroundSync.Registration.OneShot", result,
                                BACKGROUND_SYNC_STATUS_MAX + 1);
      UMA_HISTOGRAM_BOOLEAN("BackgroundSync.Registration.OneShot.CouldFire",
                            registration_could_fire == REGISTRATION_COULD_FIRE);
      UMA_HISTOGRAM_BOOLEAN(
          "BackgroundSync.Registration.OneShot.IsDuplicate",
          registration_is_duplicate == REGISTRATION_IS_DUPLICATE);
      return;
    case SYNC_PERIODIC:
      UMA_HISTOGRAM_ENUMERATION("BackgroundSync.Registration.Periodic", result,
                                BACKGROUND_SYNC_STATUS_MAX + 1);
      UMA_HISTOGRAM_BOOLEAN(
          "BackgroundSync.Registration.Periodic.IsDuplicate",
          registration_is_duplicate == REGISTRATION_IS_DUPLICATE);
      return;
  }
  NOTREACHED();
}

void BackgroundSyncMetrics::CountUnregister(SyncPeriodicity periodicity,
                                            BackgroundSyncStatus result) {
  switch (periodicity) {
    case SYNC_ONE_SHOT:
      UMA_HISTOGRAM_ENUMERATION("BackgroundSync.Unregistration.OneShot", result,
                                BACKGROUND_SYNC_STATUS_MAX + 1);
      return;
    case SYNC_PERIODIC:
      UMA_HISTOGRAM_ENUMERATION("BackgroundSync.Unregistration.Periodic",
                                result, BACKGROUND_SYNC_STATUS_MAX + 1);
      return;
  }
  NOTREACHED();
}

}  // namespace content

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

SyncTimingHelper::SyncTimingHelper() {}

SyncTimingHelper::~SyncTimingHelper() {}

// static
base::TimeDelta SyncTimingHelper::TimeSyncCycle(
    ProfileSyncServiceHarness* client) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(AwaitCommitActivityCompletion(client->service()));
  return base::Time::Now() - start;
}

// static
base::TimeDelta SyncTimingHelper::TimeMutualSyncCycle(
    ProfileSyncServiceHarness* client, ProfileSyncServiceHarness* partner) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(client->AwaitMutualSyncCycleCompletion(partner));
  return base::Time::Now() - start;
}

// static
base::TimeDelta SyncTimingHelper::TimeUntilQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(ProfileSyncServiceHarness::AwaitQuiescence(clients));
  return base::Time::Now() - start;
}

// static
void SyncTimingHelper::PrintResult(const std::string& measurement,
                                   const std::string& trace,
                                   const base::TimeDelta& dt) {
  printf("*RESULT %s: %s= %s ms\n", measurement.c_str(), trace.c_str(),
         base::IntToString(dt.InMillisecondsF()).c_str());
}

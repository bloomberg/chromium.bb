// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_timing_helper.h"

#include "base/time.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

LiveSyncTimingHelper::LiveSyncTimingHelper() {}

LiveSyncTimingHelper::~LiveSyncTimingHelper() {}

base::TimeDelta LiveSyncTimingHelper::TimeSyncCycle(
    ProfileSyncServiceHarness* client) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(client->AwaitSyncCycleCompletion("Timing sync cycle."));
  return base::Time::Now() - start;
}

base::TimeDelta LiveSyncTimingHelper::TimeMutualSyncCycle(
    ProfileSyncServiceHarness* client, ProfileSyncServiceHarness* partner) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(client->AwaitMutualSyncCycleCompletion(partner));
  return base::Time::Now() - start;
}

base::TimeDelta LiveSyncTimingHelper::TimeUntilQuiescence(
    std::vector<ProfileSyncServiceHarness*>& clients) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(ProfileSyncServiceHarness::AwaitQuiescence(clients));
  return base::Time::Now() - start;
}

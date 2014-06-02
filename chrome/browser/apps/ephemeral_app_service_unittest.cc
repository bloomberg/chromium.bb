// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Generate a time N number of days before the reference time.
// The generated time can be randomized.
base::Time GetPastTime(const base::Time& reference_time,
                       int days_before,
                       bool randomize_time = false) {
  base::Time generated_time =
      reference_time - base::TimeDelta::FromDays(days_before);

  // Add an hour so that the time is well within the number of days before.
  generated_time += base::TimeDelta::FromHours(1);

  // Add a random number of seconds between 0 - 10 hours.
  if (randomize_time)
    generated_time += base::TimeDelta::FromSeconds(base::RandInt(0, 36000));

  return generated_time;
}

}  // namespace

class EphemeralAppServiceTest : public testing::Test {
 protected:
  typedef EphemeralAppService::LaunchTimeAppMap LaunchTimeAppMap;

  EphemeralAppServiceTest() {}
  virtual ~EphemeralAppServiceTest() {}

  void RunTest(int ephemeral_app_count,
               const LaunchTimeAppMap& launch_times,
               const std::set<std::string>& expected_removed_ids) {
    std::set<std::string> remove_app_ids;
    EphemeralAppService::GetAppsToRemove(ephemeral_app_count,
                                         launch_times,
                                         &remove_app_ids);
    EXPECT_EQ(expected_removed_ids, remove_app_ids);
  }

  void RunTestCheckLRU(int ephemeral_app_count,
                       LaunchTimeAppMap& launch_times,
                       int expected_removed_count) {
    std::set<std::string> remove_app_ids;
    EphemeralAppService::GetAppsToRemove(ephemeral_app_count,
                                         launch_times,
                                         &remove_app_ids);
    EXPECT_EQ(expected_removed_count, (int) remove_app_ids.size());

    // Move the launch times of removed apps to another map.
    LaunchTimeAppMap removed_apps;
    for (LaunchTimeAppMap::iterator it = launch_times.begin();
         it != launch_times.end(); ) {
      if (remove_app_ids.find(it->second) != remove_app_ids.end()) {
        removed_apps.insert(*it);
        launch_times.erase(it++);
      } else {
        ++it;
      }
    }

    if (launch_times.empty())
      return;

    // Verify that the removed apps have launch times earlier than or equal to
    // all retained apps. We can actually just compare with the first entry in
    // |launch_times| but will make no implementation assumptions.
    for (LaunchTimeAppMap::const_iterator removed = removed_apps.begin();
         removed != removed_apps.end(); ++removed) {
      for (LaunchTimeAppMap::iterator retained = launch_times.begin();
         retained != launch_times.end(); ++retained) {
        EXPECT_LE(removed->first, retained->first);
      }
    }
  }

  // Generate X app launch times, N days before the reference time.
  // If |generated_ids| is not NULL, the generated app IDs will be added
  // to the set.
  void GenerateLaunchTimes(const base::Time& reference_time,
                           int days_before,
                           int count,
                           LaunchTimeAppMap* launch_times,
                           std::set<std::string>* generated_ids = NULL) {
    for (int i = 0; i < count; ++i) {
      std::string app_id = base::IntToString(launch_times->size());
      launch_times->insert(std::make_pair(
          GetPastTime(reference_time, days_before, true),
          app_id));

      if (generated_ids)
        generated_ids->insert(app_id);
    }
  }

  // Add inactive apps that should always be removed by garbage collection.
  void AddInactiveApps(const base::Time& reference_time,
                       int count,
                       LaunchTimeAppMap* launch_times,
                       std::set<std::string>* generated_ids = NULL) {
    GenerateLaunchTimes(reference_time,
                        EphemeralAppService::kAppInactiveThreshold + 1,
                        count,
                        launch_times,
                        generated_ids);
  }

  // Add recently launched apps that should NOT be removed by garbage
  // collection regardless of the number of cached ephemeral apps.
  void AddRecentlyLaunchedApps(const base::Time& reference_time,
                               int count,
                               LaunchTimeAppMap* launch_times,
                               std::set<std::string>* generated_ids = NULL) {
    GenerateLaunchTimes(reference_time,
                        EphemeralAppService::kAppKeepThreshold,
                        count,
                        launch_times,
                        generated_ids);
  }

  // Add apps launched between the kAppInactiveThreshold and kAppKeepThreshold,
  // which may or may not be removed by garbage collection depending on the
  // number of ephemeral apps in the cache.
  void AddIntermediateApps(const base::Time& reference_time,
                           int count,
                           LaunchTimeAppMap* launch_times,
                           std::set<std::string>* generated_ids = NULL) {
    int days_before = base::RandInt(EphemeralAppService::kAppKeepThreshold + 1,
                                    EphemeralAppService::kAppInactiveThreshold);
    GenerateLaunchTimes(reference_time,
                        days_before,
                        count,
                        launch_times,
                        generated_ids);
  }
};

// Verify that inactive apps are removed even if the cache has not reached
// capacity.
// Test case: | inactive |
// Expected output: All inactive apps removed.
TEST_F(EphemeralAppServiceTest, RemoveInactiveApps) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;
  std::set<std::string> expected_removed_ids;

  AddInactiveApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount / 5,
      &launch_times,
      &expected_removed_ids);
  RunTest(launch_times.size(), launch_times, expected_removed_ids);
}

// Verify that inactive apps are removed even if the cache has not reached
// capacity.
// Test case: | inactive | intermediate | recently launched |
// Expected output: All inactive apps removed, other apps retained.
TEST_F(EphemeralAppServiceTest, RemoveInactiveAppsKeepOthers) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;
  std::set<std::string> expected_removed_ids;

  AddInactiveApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount / 5,
      &launch_times,
      &expected_removed_ids);
  AddIntermediateApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount / 5,
      &launch_times);
  AddRecentlyLaunchedApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount / 5,
      &launch_times);
  RunTest(launch_times.size(), launch_times, expected_removed_ids);
}

// Verify that recently launched apps will not be removed, even when the cache
// overflows.
// Test case: | recently launched |
// Expected output: All recently launched apps retained.
TEST_F(EphemeralAppServiceTest, KeepRecentLaunch) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;

  AddRecentlyLaunchedApps(
      time_now,
      3,
      &launch_times);
  RunTest(launch_times.size(), launch_times, std::set<std::string>());

  AddRecentlyLaunchedApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount,
      &launch_times); // overflow
  RunTest(launch_times.size(), launch_times, std::set<std::string>());
}

// Verify that recently launched apps will not be removed, even when the cache
// overflows.
// Test case: | intermediate (overflow) | recently launched (overflow) |
// Expected output: All recently launched apps retained, intermediate apps
// removed.
TEST_F(EphemeralAppServiceTest, KeepRecentLaunchRemoveOthers) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;
  std::set<std::string> expected_removed_ids;

  AddRecentlyLaunchedApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount + 3,
      &launch_times); // overflow
  AddIntermediateApps(
      time_now,
      3,
      &launch_times,
      &expected_removed_ids); // overflow
  RunTest(launch_times.size(), launch_times, expected_removed_ids);
}

// Verify that the LRU algorithm is implemented correctly.
// Test case: | intermediate (overflow) |
// Expected output: The least recently launched apps are removed.
TEST_F(EphemeralAppServiceTest, RemoveOverflow) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;

  const int kOverflow = 3;
  AddIntermediateApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount + kOverflow,
      &launch_times); // overflow
  RunTestCheckLRU(launch_times.size(), launch_times, kOverflow);
}

// Verify that GetAppsToRemove() takes into account the number of running apps,
// since they are not included in the launch times.
// Test case: | intermediate (overflow) | running apps |
// Expected output: The least recently launched apps are removed.
TEST_F(EphemeralAppServiceTest, RemoveOverflowWithRunningApps) {
  base::Time time_now = base::Time::Now();
  LaunchTimeAppMap launch_times;

  const int kRunningApps = 3;
  AddIntermediateApps(
      time_now,
      EphemeralAppService::kMaxEphemeralAppsCount,
      &launch_times);  // overflow
  RunTestCheckLRU(
      launch_times.size() + kRunningApps,
      launch_times,
      kRunningApps);
}

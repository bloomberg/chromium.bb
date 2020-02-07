// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_app_availability_tracker.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"

namespace openscreen {
namespace cast {
namespace {

using ::testing::UnorderedElementsAreArray;

MATCHER_P(CastMediaSourcesEqual, expected, "") {
  if (expected.size() != arg.size())
    return false;
  return std::equal(
      expected.begin(), expected.end(), arg.begin(),
      [](const CastMediaSource& source1, const CastMediaSource& source2) {
        return source1.source_id() == source2.source_id();
      });
}

}  // namespace

class CastAppAvailabilityTrackerTest : public ::testing::Test {
 public:
  CastAppAvailabilityTrackerTest() : clock_(Clock::now()) {}
  ~CastAppAvailabilityTrackerTest() override = default;

  Clock::time_point Now() const { return clock_.now(); }

 protected:
  FakeClock clock_;
  CastAppAvailabilityTracker tracker_;
};

TEST_F(CastAppAvailabilityTrackerTest, RegisterSource) {
  CastMediaSource source1("cast:AAA?clientId=1", {"AAA"});
  CastMediaSource source2("cast:AAA?clientId=2", {"AAA"});

  std::vector<std::string> expected_app_ids = {"AAA"};
  EXPECT_EQ(expected_app_ids, tracker_.RegisterSource(source1));

  EXPECT_EQ(std::vector<std::string>{}, tracker_.RegisterSource(source1));
  EXPECT_EQ(std::vector<std::string>{}, tracker_.RegisterSource(source2));

  tracker_.UnregisterSource(source1);
  tracker_.UnregisterSource(source2);

  EXPECT_EQ(expected_app_ids, tracker_.RegisterSource(source1));
  EXPECT_EQ(expected_app_ids, tracker_.GetRegisteredApps());
}

TEST_F(CastAppAvailabilityTrackerTest, RegisterSourceReturnsMultipleAppIds) {
  CastMediaSource source1("urn:x-org.chromium.media:source:tab:1",
                          {"0F5096E8", "85CDB22F"});

  // Mirorring app ids.
  std::vector<std::string> expected_app_ids = {"0F5096E8", "85CDB22F"};
  EXPECT_THAT(tracker_.RegisterSource(source1),
              UnorderedElementsAreArray(expected_app_ids));
  EXPECT_THAT(tracker_.GetRegisteredApps(),
              UnorderedElementsAreArray(expected_app_ids));
}

TEST_F(CastAppAvailabilityTrackerTest, MultipleAppIdsAlreadyTrackingOne) {
  // One of the mirroring app IDs.
  CastMediaSource source1("cast:0F5096E8?clientId=123", {"0F5096E8"});

  std::vector<std::string> new_app_ids = {"0F5096E8"};
  std::vector<std::string> registered_app_ids = {"0F5096E8"};
  EXPECT_EQ(new_app_ids, tracker_.RegisterSource(source1));
  EXPECT_EQ(registered_app_ids, tracker_.GetRegisteredApps());

  CastMediaSource source2("urn:x-org.chromium.media:source:tab:1",
                          {"0F5096E8", "85CDB22F"});

  new_app_ids = {"85CDB22F"};
  registered_app_ids = {"0F5096E8", "85CDB22F"};

  EXPECT_EQ(new_app_ids, tracker_.RegisterSource(source2));
  EXPECT_THAT(tracker_.GetRegisteredApps(),
              UnorderedElementsAreArray(registered_app_ids));
}

TEST_F(CastAppAvailabilityTrackerTest, UpdateAppAvailability) {
  CastMediaSource source1("cast:AAA?clientId=1", {"AAA"});
  CastMediaSource source2("cast:AAA?clientId=2", {"AAA"});
  CastMediaSource source3("cast:BBB?clientId=3", {"BBB"});

  tracker_.RegisterSource(source3);

  // |source3| not affected.
  EXPECT_THAT(
      tracker_.UpdateAppAvailability(
          "deviceId1", "AAA", {AppAvailabilityResult::kAvailable, Now()}),
      CastMediaSourcesEqual(std::vector<CastMediaSource>()));

  std::vector<std::string> devices_1 = {"deviceId1"};
  std::vector<std::string> devices_1_2 = {"deviceId1", "deviceId2"};
  std::vector<CastMediaSource> sources_1 = {source1};
  std::vector<CastMediaSource> sources_1_2 = {source1, source2};

  // Tracker returns available devices even though sources aren't registered.
  EXPECT_EQ(devices_1, tracker_.GetAvailableDevices(source1));
  EXPECT_EQ(devices_1, tracker_.GetAvailableDevices(source2));
  EXPECT_TRUE(tracker_.GetAvailableDevices(source3).empty());

  tracker_.RegisterSource(source1);
  // Only |source1| is registered for this app.
  EXPECT_THAT(
      tracker_.UpdateAppAvailability(
          "deviceId2", "AAA", {AppAvailabilityResult::kAvailable, Now()}),
      CastMediaSourcesEqual(sources_1));
  EXPECT_THAT(tracker_.GetAvailableDevices(source1),
              UnorderedElementsAreArray(devices_1_2));
  EXPECT_THAT(tracker_.GetAvailableDevices(source2),
              UnorderedElementsAreArray(devices_1_2));
  EXPECT_TRUE(tracker_.GetAvailableDevices(source3).empty());

  tracker_.RegisterSource(source2);
  EXPECT_THAT(
      tracker_.UpdateAppAvailability(
          "deviceId2", "AAA", {AppAvailabilityResult::kUnavailable, Now()}),
      CastMediaSourcesEqual(sources_1_2));
  EXPECT_EQ(devices_1, tracker_.GetAvailableDevices(source1));
  EXPECT_EQ(devices_1, tracker_.GetAvailableDevices(source2));
  EXPECT_TRUE(tracker_.GetAvailableDevices(source3).empty());
}

TEST_F(CastAppAvailabilityTrackerTest, RemoveResultsForDevice) {
  CastMediaSource source1("cast:AAA?clientId=1", {"AAA"});

  tracker_.UpdateAppAvailability("deviceId1", "AAA",
                                 {AppAvailabilityResult::kAvailable, Now()});
  EXPECT_EQ(AppAvailabilityResult::kAvailable,
            tracker_.GetAvailability("deviceId1", "AAA").availability);

  std::vector<std::string> expected_device_ids = {"deviceId1"};
  EXPECT_EQ(expected_device_ids, tracker_.GetAvailableDevices(source1));

  // Unrelated device ID.
  tracker_.RemoveResultsForDevice("deviceId2");
  EXPECT_EQ(AppAvailabilityResult::kAvailable,
            tracker_.GetAvailability("deviceId1", "AAA").availability);
  EXPECT_EQ(expected_device_ids, tracker_.GetAvailableDevices(source1));

  tracker_.RemoveResultsForDevice("deviceId1");
  EXPECT_EQ(AppAvailabilityResult::kUnknown,
            tracker_.GetAvailability("deviceId1", "AAA").availability);
  EXPECT_EQ(std::vector<std::string>{}, tracker_.GetAvailableDevices(source1));
}

}  // namespace cast
}  // namespace openscreen

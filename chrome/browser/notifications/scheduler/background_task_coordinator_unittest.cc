// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/background_task_coordinator.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/time/clock.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/notification_entry.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace notifications {
namespace {

using Notifications = BackgroundTaskCoordinator::Notifications;
using ClientStates = BackgroundTaskCoordinator::ClientStates;

const char kGuid[] = "1234";
const std::vector<test::ImpressionTestData> kSingleClientImpressionTestData = {
    {SchedulerClientType::kTest1,
     1 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

const std::vector<test::ImpressionTestData> kClientsImpressionTestData = {
    {SchedulerClientType::kTest1,
     1 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest2,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

class MockNotificationBackgroundTaskScheduler
    : public NotificationBackgroundTaskScheduler {
 public:
  MockNotificationBackgroundTaskScheduler() = default;
  ~MockNotificationBackgroundTaskScheduler() override = default;
  MOCK_METHOD3(Schedule,
               void(notifications::SchedulerTaskTime,
                    base::TimeDelta,
                    base::TimeDelta));
  MOCK_METHOD0(Cancel, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotificationBackgroundTaskScheduler);
};

// Clock to mock Clock::Now() to get a fixed time in the test.
class FakeClock : public base::Clock {
 public:
  FakeClock() = default;
  ~FakeClock() override = default;

  void SetTime(const base::Time& time) { time_ = time; }

 private:
  // base::Clock implementation.
  base::Time Now() const override { return time_; }

  base::Time time_;
  DISALLOW_COPY_AND_ASSIGN(FakeClock);
};

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;

  // The type of current background task.
  SchedulerTaskTime task_start_time = SchedulerTaskTime::kMorning;
};

class BackgroundTaskCoordinatorTest : public testing::Test {
 public:
  BackgroundTaskCoordinatorTest() = default;
  ~BackgroundTaskCoordinatorTest() override = default;

 protected:
  void SetUp() override {
    // Setup configuration used by this test.
    config_.morning_task_hour = 6;
    config_.evening_task_hour = 18;
    config_.max_daily_shown_all_type = 3;
    config_.max_daily_shown_per_type = 2;
    config_.suppression_duration = base::TimeDelta::FromDays(3);

    auto background_task =
        std::make_unique<MockNotificationBackgroundTaskScheduler>();
    background_task_ = background_task.get();
    coordinator_ = std::make_unique<BackgroundTaskCoordinator>(
        std::move(background_task), &config_, &clock_);
  }

  MockNotificationBackgroundTaskScheduler* background_task() {
    return background_task_;
  }

  SchedulerConfig* config() { return &config_; }

  void SetNow(const char* now_str) {
    base::Time now = GetTime(now_str);
    clock_.SetTime(now);
  }

  base::Time GetTime(const char* time_str) {
    base::Time time;
    bool success = base::Time::FromString(time_str, &time);
    DCHECK(success);
    return time;
  }

  void ScheduleTask(const TestData& test_data) {
    test_data_ = test_data;
    test::AddImpressionTestData(test_data_.impression_test_data,
                                &client_states_);
    std::map<SchedulerClientType, const ClientState*> client_states;
    for (const auto& type : client_states_) {
      client_states.emplace(type.first, type.second.get());
    }

    Notifications notifications;
    for (const auto& entry : test_data_.notification_entries) {
      notifications[entry.type].emplace_back(&entry);
    }
    coordinator_->ScheduleBackgroundTask(std::move(notifications),
                                         std::move(client_states),
                                         test_data_.task_start_time);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakeClock clock_;
  SchedulerConfig config_;
  std::unique_ptr<BackgroundTaskCoordinator> coordinator_;
  MockNotificationBackgroundTaskScheduler* background_task_;
  TestData test_data_;
  std::map<SchedulerClientType, std::unique_ptr<ClientState>> client_states_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinatorTest);
};

// No notification persisted, then no background task needs to be scheduled.
// And current task should be canceled.
TEST_F(BackgroundTaskCoordinatorTest, NoNotification) {
  EXPECT_CALL(*background_task(), Cancel());
  EXPECT_CALL(*background_task(), Schedule(_, _, _)).Times(0);
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  ScheduleTask(test_data);
}

// In a morning task, find one notification and schedule an evening task.
TEST_F(BackgroundTaskCoordinatorTest, InMorningScheduleEvening) {
  const char kNow[] = "04/25/20 01:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task this evening.
  auto expected_window_start = GetTime("04/25/20 18:00:00 PM") - GetTime(kNow);
  EXPECT_CALL(*background_task(),
              Schedule(_, expected_window_start,
                       expected_window_start +
                           config()->background_task_window_duration));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// In morning task, schedule evening task but throttled, schedule to next
// morning.
TEST_F(BackgroundTaskCoordinatorTest, InMorningScheduleEveningThrottled) {
  const char kNow[] = "04/25/20 02:00:00 PM";
  SetNow(kNow);

  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  EXPECT_CALL(*background_task(),
              Schedule(_, GetTime("04/26/20 06:00:00 AM") - GetTime(kNow), _));

  auto impression_data = kSingleClientImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");
  impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// In an evening task, schedule background task to run next morning.
TEST_F(BackgroundTaskCoordinatorTest, InEveningScheduleNextMorning) {
  const char kNow[] = "04/25/20 18:00:00 PM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kEvening};
  ScheduleTask(test_data);
}

// In an evening task, schedule background task to run next morning, even if we
// reached the daily max.
TEST_F(BackgroundTaskCoordinatorTest, InEveningScheduleNextMorningThrottled) {
  const char kNow[] = "04/25/20 18:00:00 PM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  // We have reached daily max.
  auto impression_data = kSingleClientImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");
  impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kEvening};
  ScheduleTask(test_data);
}

// Suppression will result in background task scheduled after suppression
// expired.
TEST_F(BackgroundTaskCoordinatorTest, Suppression) {
  const char kNow[] = "04/25/20 06:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task in the morning after suppression expired.
  auto expected_window_start = GetTime("04/28/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kSingleClientImpressionTestData;
  impression_data.back().suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If two different types want to schedule at different times, pick the earilier
// one.
TEST_F(BackgroundTaskCoordinatorTest, ScheduleEarlierTime) {
  const char kNow[] = "04/25/20 01:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // kTest1 type will run this evening, kTest2 will run task 3 days later.
  // Expected to run the earilier task.
  auto expected_window_start = GetTime("04/25/20 18:00:00 PM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  NotificationEntry entry1(SchedulerClientType::kTest1, kGuid);
  NotificationEntry entry2(SchedulerClientType::kTest2, "guid_entry2");
  auto impression_data = kClientsImpressionTestData;
  impression_data[0].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));
  TestData test_data{
      impression_data, {entry1, entry2}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If reached |max_daily_shown_all_type|, background task should run tomorrow.
TEST_F(BackgroundTaskCoordinatorTest, InMorningThrottledAllTypes) {
  const char kNow[] = "04/25/20 05:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kClientsImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");

  // Make sure we reach daily max for all types.
  for (int i = 0; i < config()->max_daily_shown_all_type; i++)
    impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If reached |max_daily_shown_all_type| and all types have suppression,
// background task should run after one suppression expired.
TEST_F(BackgroundTaskCoordinatorTest, ThrottledAllTypesAndSuppression) {
  const char kNow[] = "04/25/20 05:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run after 3 days suppression ends.
  auto expected_window_start = GetTime("04/28/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kClientsImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");

  // Make sure we reach daily max for all types.
  for (int i = 0; i < config()->max_daily_shown_all_type; i++)
    impression_data[1].impressions.emplace_back(impression);

  // Suppression for both types.
  impression_data[0].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));
  impression_data[1].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(4));

  NotificationEntry entry1(SchedulerClientType::kTest1, "test_guid_1");
  NotificationEntry entry2(SchedulerClientType::kTest2, "test_guid_2");
  TestData test_data{
      impression_data, {entry1, entry2}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

}  // namespace
}  // namespace notifications

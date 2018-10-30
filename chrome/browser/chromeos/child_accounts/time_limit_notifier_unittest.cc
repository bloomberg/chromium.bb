// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_notifier.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TimeLimitNotifierTest : public testing::Test {
 public:
  TimeLimitNotifierTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        notification_tester_(&profile_),
        notifier_(&profile_, task_runner_) {}
  ~TimeLimitNotifierTest() override = default;

 protected:
  bool HasNotification() {
    return notification_tester_.GetNotification("time-limit-notification")
        .has_value();
  }

  void RemoveNotification() {
    notification_tester_.RemoveAllNotifications(
        NotificationHandler::Type::TRANSIENT, true /* by_user */);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  content::TestBrowserThreadBundle bundle_;
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_tester_;
  TimeLimitNotifier notifier_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeLimitNotifierTest);
};

TEST_F(TimeLimitNotifierTest, ShowNotifications) {
  notifier_.MaybeScheduleNotifications(
      TimeLimitNotifier::LimitType::kScreenTime,
      base::TimeDelta::FromMinutes(20));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(10));
  EXPECT_FALSE(HasNotification());

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_TRUE(HasNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(4));
  EXPECT_TRUE(HasNotification());
}

TEST_F(TimeLimitNotifierTest, DismissNotification) {
  notifier_.MaybeScheduleNotifications(TimeLimitNotifier::LimitType::kBedTime,
                                       base::TimeDelta::FromMinutes(10));

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_TRUE(HasNotification());
  RemoveNotification();

  // Fast forward one minute; the same notification is not reshown.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_FALSE(HasNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(3));
  EXPECT_TRUE(HasNotification());
}

TEST_F(TimeLimitNotifierTest, OnlyExitNotification) {
  notifier_.MaybeScheduleNotifications(
      TimeLimitNotifier::LimitType::kScreenTime,
      base::TimeDelta::FromMinutes(3));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_FALSE(HasNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(HasNotification());
  RemoveNotification();

  // Only one notification was shown.
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasNotification());
}

TEST_F(TimeLimitNotifierTest, NoNotifications) {
  notifier_.MaybeScheduleNotifications(TimeLimitNotifier::LimitType::kBedTime,
                                       base::TimeDelta::FromSeconds(30));

  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasNotification());
}

TEST_F(TimeLimitNotifierTest, UnscheduleNotifications) {
  notifier_.MaybeScheduleNotifications(
      TimeLimitNotifier::LimitType::kScreenTime,
      base::TimeDelta::FromMinutes(10));

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_TRUE(HasNotification());
  RemoveNotification();

  // Stop the timers.
  notifier_.UnscheduleNotifications();
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasNotification());
}

TEST_F(TimeLimitNotifierTest, RescheduleNotifications) {
  notifier_.MaybeScheduleNotifications(
      TimeLimitNotifier::LimitType::kScreenTime,
      base::TimeDelta::FromMinutes(20));

  // Update the notifier with a new limit.
  notifier_.MaybeScheduleNotifications(
      TimeLimitNotifier::LimitType::kScreenTime,
      base::TimeDelta::FromMinutes(30));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(20));
  EXPECT_FALSE(HasNotification());

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_TRUE(HasNotification());
  RemoveNotification();

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(4));
  EXPECT_TRUE(HasNotification());
}

}  // namespace chromeos

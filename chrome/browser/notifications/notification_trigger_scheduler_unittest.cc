// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/notifications/notification_trigger_scheduler.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

constexpr base::TimeDelta kTimeAdvance = base::TimeDelta::FromMilliseconds(1);

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

PlatformNotificationServiceImpl* GetNotificationService() {
  return PlatformNotificationServiceImpl::GetInstance();
}

class MockNotificationTriggerScheduler : public NotificationTriggerScheduler {
 public:
  ~MockNotificationTriggerScheduler() override = default;
  MOCK_METHOD1(TriggerNotificationsForStoragePartition,
               void(content::StoragePartition* partition));
};

}  // namespace

class NotificationTriggerSchedulerTest : public testing::Test {
 protected:
  NotificationTriggerSchedulerTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::NowSource::
                MAIN_THREAD_MOCK_TIME) {}

  void SetUp() override {
    scheduler_ = new MockNotificationTriggerScheduler();
    GetNotificationService()->trigger_scheduler_ = base::WrapUnique(scheduler_);

    // Advance time a little bit so TimeTicks::Now().is_null() becomes false.
    thread_bundle_.FastForwardBy(kTimeAdvance);
  }

  MockNotificationTriggerScheduler* scheduler_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(NotificationTriggerSchedulerTest,
       TriggerNotificationsCallsAllStoragePartitions) {
  EXPECT_CALL(*scheduler_, TriggerNotificationsForStoragePartition(_)).Times(0);

  std::unique_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  Profile* profile1 = profile_manager->CreateTestingProfile("profile1");
  Profile* profile2 = profile_manager->CreateTestingProfile("profile2");

  auto* partition1 = content::BrowserContext::GetStoragePartitionForSite(
      profile1, GURL("http://example.com"));
  auto* partition2 = content::BrowserContext::GetStoragePartitionForSite(
      profile2, GURL("http://example.com"));

  auto now = base::Time::Now();
  auto delta = base::TimeDelta::FromSeconds(3);
  GetNotificationService()->ScheduleTrigger(profile1, now + delta);
  GetNotificationService()->ScheduleTrigger(profile2, now + delta);
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(scheduler_);
  EXPECT_CALL(*scheduler_, TriggerNotificationsForStoragePartition(partition1));
  EXPECT_CALL(*scheduler_, TriggerNotificationsForStoragePartition(partition2));

  thread_bundle_.FastForwardBy(delta);
  base::RunLoop().RunUntilIdle();
}

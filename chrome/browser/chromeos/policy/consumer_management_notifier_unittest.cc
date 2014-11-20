// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_notifier.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ConsumerManagementNotifierTest : public BrowserWithTestWindowTest {
 public:
  ConsumerManagementNotifierTest()
      : fake_service_(new FakeConsumerManagementService()) {
    fake_service_->SetStatusAndEnrollmentStage(
        ConsumerManagementService::STATUS_UNENROLLED,
        ConsumerManagementService::ENROLLMENT_STAGE_NONE);

    // Inject objects.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetConsumerManagementServiceForTesting(
        make_scoped_ptr(fake_service_));
  }

  virtual void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set up TestingProfileManager. This is required for NotificationUIManager.
    testing_profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  virtual void TearDown() override {
    if (notification_)
      notification_->Shutdown();
    notification_.reset();
    g_browser_process->notification_ui_manager()->CancelAll();
    testing_profile_manager_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  void CreateConsumerManagementNotifier() {
    notification_.reset(
      new ConsumerManagementNotifier(profile(), fake_service_));
  }

  bool HasEnrollmentNotification() {
    return g_browser_process->notification_ui_manager()->FindById(
        "consumer_management.enroll",
        NotificationUIManager::GetProfileID(profile()));
  }

  FakeConsumerManagementService* fake_service_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  scoped_ptr<ConsumerManagementNotifier> notification_;
};

TEST_F(ConsumerManagementNotifierTest, ShowsNotificationWhenCreated) {
  fake_service_->SetStatusAndEnrollmentStage(
      ConsumerManagementService::STATUS_UNENROLLED,
      ConsumerManagementService::ENROLLMENT_STAGE_CANCELED);
  EXPECT_FALSE(HasEnrollmentNotification());

  CreateConsumerManagementNotifier();

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

TEST_F(ConsumerManagementNotifierTest, ShowsNotificationWhenStatusChanged) {
  fake_service_->SetStatusAndEnrollmentStage(
      ConsumerManagementService::STATUS_ENROLLING,
      ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED);

  CreateConsumerManagementNotifier();
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED,
            fake_service_->GetEnrollmentStage());
  EXPECT_FALSE(HasEnrollmentNotification());

  fake_service_->SetStatusAndEnrollmentStage(
      ConsumerManagementService::STATUS_ENROLLED,
      ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS);
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

}  // namespace policy

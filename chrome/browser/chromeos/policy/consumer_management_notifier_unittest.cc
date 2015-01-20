// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
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
    fake_service_->SetStatusAndStage(
        ConsumerManagementService::STATUS_UNENROLLED,
        ConsumerManagementStage::None());

    // Inject objects.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetConsumerManagementServiceForTesting(
        make_scoped_ptr(fake_service_));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set up TestingProfileManager. This is required for NotificationUIManager.
    testing_profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  void TearDown() override {
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

  bool HasUnenrollmentNotification() {
    return g_browser_process->notification_ui_manager()->FindById(
        "consumer_management.unenroll",
        NotificationUIManager::GetProfileID(profile()));
  }

  FakeConsumerManagementService* fake_service_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  scoped_ptr<ConsumerManagementNotifier> notification_;
};

TEST_F(ConsumerManagementNotifierTest,
       ShowsEnrollmentNotificationWhenCreated) {
  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_UNENROLLED,
      ConsumerManagementStage::EnrollmentCanceled());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());

  CreateConsumerManagementNotifier();

  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());
  EXPECT_TRUE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());
}

TEST_F(ConsumerManagementNotifierTest,
       ShowsUnenrollmentNotificationWhenCreated) {
  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_UNENROLLED,
      ConsumerManagementStage::UnenrollmentSuccess());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());

  CreateConsumerManagementNotifier();

  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_TRUE(HasUnenrollmentNotification());
}

TEST_F(ConsumerManagementNotifierTest,
       ShowsEnrollmentNotificationWhenStatusChanged) {
  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_ENROLLING,
      ConsumerManagementStage::EnrollmentOwnerStored());

  CreateConsumerManagementNotifier();
  EXPECT_EQ(ConsumerManagementStage::EnrollmentOwnerStored(),
            fake_service_->GetStage());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());

  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_ENROLLED,
      ConsumerManagementStage::EnrollmentSuccess());
  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());
  EXPECT_TRUE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());
}

TEST_F(ConsumerManagementNotifierTest,
       ShowsUnenrollmentNotificationWhenStatusChanged) {
  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_ENROLLED,
      ConsumerManagementStage::None());

  CreateConsumerManagementNotifier();
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());

  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_UNENROLLING,
      ConsumerManagementStage::UnenrollmentRequested());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_FALSE(HasUnenrollmentNotification());

  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_UNENROLLED,
      ConsumerManagementStage::UnenrollmentSuccess());
  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_TRUE(HasUnenrollmentNotification());
}

}  // namespace policy

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kTestOwner = "test.owner@chromium.org.test";
const char* kTestUser = "test.user@chromium.org.test";
}

namespace policy {

class ConsumerEnrollmentHandlerFactoryTest : public testing::Test {
 public:
  ConsumerEnrollmentHandlerFactoryTest()
      : fake_service_(new FakeConsumerManagementService()),
        fake_user_manager_(new chromeos::FakeChromeUserManager()),
        scoped_user_manager_enabler_(fake_user_manager_),
        testing_profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {
    // Set up FakeConsumerManagementService.
    fake_service_->SetStatusAndStage(
        ConsumerManagementService::STATUS_ENROLLING,
        ConsumerManagementStage::EnrollmentOwnerStored());

    // Inject fake objects.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetConsumerManagementServiceForTesting(
        make_scoped_ptr(fake_service_));

    // Set up FakeChromeUserManager.
    fake_user_manager_->AddUser(AccountId::FromUserEmail(kTestOwner));
    fake_user_manager_->AddUser(AccountId::FromUserEmail(kTestUser));
    fake_user_manager_->set_owner_id(AccountId::FromUserEmail(kTestOwner));
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  FakeConsumerManagementService* fake_service_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  chromeos::ScopedUserManagerEnabler scoped_user_manager_enabler_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
};

TEST_F(ConsumerEnrollmentHandlerFactoryTest, ServiceIsCreated) {
  Profile* profile = testing_profile_manager_->CreateTestingProfile(kTestOwner);
  EXPECT_TRUE(ConsumerEnrollmentHandlerFactory::GetForBrowserContext(profile));
}

TEST_F(ConsumerEnrollmentHandlerFactoryTest, ServiceIsNotCreatedForNonOwner) {
  Profile* profile = testing_profile_manager_->CreateTestingProfile(kTestUser);
  EXPECT_FALSE(ConsumerEnrollmentHandlerFactory::GetForBrowserContext(profile));
}

TEST_F(ConsumerEnrollmentHandlerFactoryTest,
       ServiceIsNotCreatedIfItHasNothingToDo) {
  fake_service_->SetStatusAndStage(
      ConsumerManagementService::STATUS_UNENROLLED,
      ConsumerManagementStage::None());

  Profile* profile = testing_profile_manager_->CreateTestingProfile(kTestOwner);
  EXPECT_FALSE(ConsumerEnrollmentHandlerFactory::GetForBrowserContext(profile));
}

}  // namespace policy

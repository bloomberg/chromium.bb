// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include <stddef.h>

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace {

const char kTestAccountId[] = "testuser@test.com";

// Notification ID corresponding to kProfileSyncNotificationId + kTestAccountId.
const char kNotificationId[] = "chrome://settings/sync/testuser@test.com";

class FakeLoginUIService: public LoginUIService {
 public:
  FakeLoginUIService() : LoginUIService(nullptr) {}
  ~FakeLoginUIService() override {}
};

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() : focus_ui_call_count_(0) {}

  ~FakeLoginUI() override {}

  int focus_ui_call_count() const { return focus_ui_call_count_; }

 private:
  // LoginUIService::LoginUI:
  void FocusUI() override { ++focus_ui_call_count_; }

  int focus_ui_call_count_;
};

std::unique_ptr<KeyedService> BuildMockLoginUIService(
    content::BrowserContext* profile) {
  return std::make_unique<FakeLoginUIService>();
}

class SyncErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  SyncErrorNotifierTest() {}
  ~SyncErrorNotifierTest() override {}

  void SetUp() override {
    DCHECK(TestingBrowserProcess::GetGlobal());

    BrowserWithTestWindowTest::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(kTestAccountId);

    service_ = std::make_unique<browser_sync::ProfileSyncServiceMock>(
        CreateProfileSyncServiceParamsForTest(profile_));

    FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
        LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_, BuildMockLoginUIService));
    login_ui_service->SetLoginUI(&login_ui_);

    error_controller_ =
        std::make_unique<syncer::SyncErrorController>(service_.get());
    error_notifier_ =
        std::make_unique<SyncErrorNotifier>(error_controller_.get(), profile_);

    notification_ui_manager_ = g_browser_process->notification_ui_manager();
  }

  void TearDown() override {
    error_notifier_->Shutdown();
    service_.reset();
    profile_manager_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Utility function to test that SyncErrorNotifier behaves correctly for the
  // given error condition.
  void VerifySyncErrorNotifierResult(GoogleServiceAuthError::State error_state,
                                     bool is_signed_in,
                                     bool is_error,
                                     bool expected_notification) {
    EXPECT_CALL(*service_, IsFirstSetupComplete())
        .WillRepeatedly(Return(is_signed_in));

    GoogleServiceAuthError auth_error(error_state);
    EXPECT_CALL(*service_, GetAuthError()).WillRepeatedly(
        ReturnRef(auth_error));

    error_controller_->OnStateChanged(service_.get());
    EXPECT_EQ(is_error, error_controller_->HasError());

    const message_center::Notification* notification =
        notification_ui_manager_->FindById(
            kNotificationId, NotificationUIManager::GetProfileID(profile_));
    if (expected_notification) {
      ASSERT_TRUE(notification);
      ASSERT_FALSE(notification->title().empty());
      ASSERT_FALSE(notification->message().empty());
      ASSERT_EQ((size_t)1, notification->buttons().size());
    } else {
      ASSERT_FALSE(notification);
    }
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<syncer::SyncErrorController> error_controller_;
  std::unique_ptr<SyncErrorNotifier> error_notifier_;
  std::unique_ptr<browser_sync::ProfileSyncServiceMock> service_;
  TestingProfile* profile_;
  FakeLoginUI login_ui_;
  NotificationUIManager* notification_ui_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifierTest);
};

// Test that SyncErrorNotifier shows an notification if a passphrase is
// required.
TEST_F(SyncErrorNotifierTest, PassphraseNotification) {
  chromeos::ScopedUserManagerEnabler scoped_enabler(
      new chromeos::MockUserManager());
  ASSERT_FALSE(notification_ui_manager_->FindById(
      kNotificationId, NotificationUIManager::GetProfileID(profile_)));

  syncer::SyncEngine::Status status;
  EXPECT_CALL(*service_, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(*service_, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  {
    SCOPED_TRACE("Expected a notification for passphrase error");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, true /* error */,
                                  true /* expecting notification */);
  }

  // Sumulate discarded notification and check that notification is not shown.
  notification_ui_manager_->CancelById(
      kNotificationId, NotificationUIManager::GetProfileID(profile_));
  {
    SCOPED_TRACE("Not expecting notification, one was already discarded");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, true /* error */,
                                  false /* not expecting notification */);
  }

  // Check that no notification is shown if there is no error.
  EXPECT_CALL(*service_, IsPassphraseRequired())
              .WillRepeatedly(Return(false));
  EXPECT_CALL(*service_, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(false));
  {
    SCOPED_TRACE("Not expecting notification since no error exists");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */, false /* no error */,
                                  false /* not expecting notification */);
  }

  // Check that no notification is shown if sync setup is not completed.
  EXPECT_CALL(*service_, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  {
    SCOPED_TRACE("Not expecting notification since sync setup is incomplete");
    VerifySyncErrorNotifierResult(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
        false /* not signed in */, false /* no error */,
        false /* not expecting notification */);
  }
}

}  // namespace

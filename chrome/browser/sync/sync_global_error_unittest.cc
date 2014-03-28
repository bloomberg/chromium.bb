// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_error_controller.h"
#include "chrome/browser/sync/sync_global_error_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace {

class FakeLoginUIService: public LoginUIService {
 public:
  FakeLoginUIService() : LoginUIService(NULL) {}
};

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() : focus_ui_call_count_(0) {}

  virtual ~FakeLoginUI() {}

  int focus_ui_call_count() const { return focus_ui_call_count_; }

 private:
  // Overridden from LoginUIService::LoginUI:
  virtual void FocusUI() OVERRIDE {
    ++focus_ui_call_count_;
  }
  virtual void CloseUI() OVERRIDE {}

  int focus_ui_call_count_;
};

KeyedService* BuildMockLoginUIService(content::BrowserContext* profile) {
  return new FakeLoginUIService();
}

// Same as BrowserWithTestWindowTest, but uses MockBrowser to test calls to
// ExecuteCommand method.
class SyncGlobalErrorTest : public BrowserWithTestWindowTest {
 public:
  SyncGlobalErrorTest() {}
  virtual ~SyncGlobalErrorTest() {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(ProfileSyncServiceMock::MakeSignedInTestingProfile());

    BrowserWithTestWindowTest::SetUp();
  }

  Profile* profile() { return profile_.get(); }

 private:
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(SyncGlobalErrorTest);
};

// Utility function to test that SyncGlobalError behaves correctly for the given
// error condition.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 FakeLoginUIService* login_ui_service,
                                 Browser* browser,
                                 SyncErrorController* error,
                                 SyncGlobalError* global_error,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(is_signed_in));

  GoogleServiceAuthError auth_error(error_state);
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));

  error->OnStateChanged();
  EXPECT_EQ(is_error, error->HasError());

  // If there is an error then a menu item and bubble view should be shown.
  EXPECT_EQ(is_error, global_error->HasMenuItem());
  EXPECT_EQ(is_error, global_error->HasBubbleView());

  // If there is an error then labels should not be empty.
  EXPECT_NE(0, global_error->MenuItemCommandID());
  EXPECT_NE(is_error, global_error->MenuItemLabel().empty());
  EXPECT_NE(is_error, global_error->GetBubbleViewAcceptButtonLabel().empty());

  // We never have a cancel button.
  EXPECT_TRUE(global_error->GetBubbleViewCancelButtonLabel().empty());
  // We always return a hardcoded title.
  EXPECT_FALSE(global_error->GetBubbleViewTitle().empty());

  // Test message handler.
  if (is_error) {
    FakeLoginUI* login_ui = static_cast<FakeLoginUI*>(
        login_ui_service->current_login_ui());
    global_error->ExecuteMenuItem(browser);
    ASSERT_GT(login_ui->focus_ui_call_count(), 0);
    global_error->BubbleViewAcceptButtonPressed(browser);
    global_error->BubbleViewDidClose(browser);
  }
}

} // namespace

// Test that SyncGlobalError shows an error if a passphrase is required.
TEST_F(SyncGlobalErrorTest, PassphraseGlobalError) {
  NiceMock<ProfileSyncServiceMock> service(profile());

  FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
      LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), BuildMockLoginUIService));
  FakeLoginUI login_ui;
  login_ui_service->SetLoginUI(&login_ui);

  SyncErrorController error(&service);
  SyncGlobalError global_error(&error, &service);

  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  VerifySyncGlobalErrorResult(
      &service, login_ui_service, browser(), &error, &global_error,
      GoogleServiceAuthError::NONE, true /* signed in*/, true /* error */);

  // Check that no menu item is shown if there is no error.
  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(false));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(false));
  VerifySyncGlobalErrorResult(
      &service, login_ui_service, browser(), &error, &global_error,
      GoogleServiceAuthError::NONE, true /* signed in */, false /* no error */);

  // Check that no menu item is shown if sync setup is not completed.
  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  VerifySyncGlobalErrorResult(
      &service, login_ui_service, browser(), &error, &global_error,
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      false /* not signed in */, false /* no error */);

  global_error.Shutdown();
}

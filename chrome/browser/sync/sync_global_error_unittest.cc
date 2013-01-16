// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error.h"

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using content::BrowserThread;

namespace {

#if 0
// TODO(altimofeev) See below.
class BrowserMock: public Browser {
 public:
  explicit BrowserMock(Type type, Profile* profile) : Browser(type, profile) {}

  MOCK_METHOD2(ExecuteCommandWithDisposition,
               void(int command_id, WindowOpenDisposition));
};
#endif

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

ProfileKeyedService* BuildMockLoginUIService(Profile* profile) {
  return new FakeLoginUIService();
}

// Same as BrowserWithTestWindowTest, but uses MockBrowser to test calls to
// ExecuteCommand method.
class SyncGlobalErrorTest : public BrowserWithTestWindowTest {
 public:
  SyncGlobalErrorTest() {}
  virtual ~SyncGlobalErrorTest() {}

#if 0
  // TODO(altimofeev): see below.
  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();

    set_profile(CreateProfile());
    set_browser(new BrowserMock(Browser::TYPE_TABBED, profile()));
    set_window(new TestBrowserWindow(browser()));
    browser()->SetWindowForTesting(window());
  }

  virtual void TearDown() OVERRIDE {
    testing::Test::TearDown();
  }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncGlobalErrorTest);
};

// Utility function to test that SyncGlobalError behaves correct for the given
// error condition.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 FakeLoginUIService* login_ui_service,
                                 Browser* browser,
                                 SyncGlobalError* error,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(is_signed_in));

  GoogleServiceAuthError auth_error(error_state);
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));

  error->OnStateChanged();

  // If there is an error then a wrench button badge, menu item, and bubble view
  // should be shown.
  EXPECT_EQ(error->HasBadge(), is_error);
  EXPECT_EQ(error->HasMenuItem() || error->HasCustomizedSyncMenuItem(),
            is_error);
  EXPECT_EQ(error->HasBubbleView(), is_error);

  // If there is an error then labels should not be empty.
  EXPECT_NE(error->MenuItemCommandID(), 0);
  EXPECT_NE(error->MenuItemLabel().empty(), is_error);
  EXPECT_NE(error->GetBubbleViewAcceptButtonLabel().empty(), is_error);

  // We never have a cancel button.
  EXPECT_TRUE(error->GetBubbleViewCancelButtonLabel().empty());
  // We always return a hardcoded title.
  EXPECT_FALSE(error->GetBubbleViewTitle().empty());

#if defined(OS_CHROMEOS)
  // TODO(altimofeev): Implement this in a way that doesn't involve subclassing
  //                   Browser or using GMock on browser/ui types which is
  //                   banned. Consider observing NOTIFICATION_APP_TERMINATING
  //                   instead.
  //                   http://crbug.com/134675
#else
#if defined(OS_CHROMEOS)
  if (error_state != GoogleServiceAuthError::NONE) {
    // In CrOS sign-in/sign-out is made to fix the error.
    EXPECT_CALL(*static_cast<BrowserMock*>(browser),
                ExecuteCommandWithDisposition(IDC_EXIT, _));
    error->ExecuteMenuItem(browser);
  }
#else
  // Test message handler.
  if (is_error) {
    FakeLoginUI* login_ui = static_cast<FakeLoginUI*>(
        login_ui_service->current_login_ui());
    error->ExecuteMenuItem(browser);
    ASSERT_GT(login_ui->focus_ui_call_count(), 0);
    error->BubbleViewAcceptButtonPressed(browser);
    error->BubbleViewDidClose(browser);
  }
#endif
#endif
}

} // namespace

// Test that SyncGlobalError shows an error if a passphrase is required.
TEST_F(SyncGlobalErrorTest, PassphraseGlobalError) {
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile.get());
  FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
      LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile.get(), BuildMockLoginUIService));
  FakeLoginUI login_ui;
  login_ui_service->SetLoginUI(&login_ui);
  SyncGlobalError error(&service, signin);

  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  VerifySyncGlobalErrorResult(
      &service, login_ui_service, browser(), &error,
      GoogleServiceAuthError::NONE, true, true);
}

// Test that SyncGlobalError shows an error for conditions that can be resolved
// by the user and suppresses errors for conditions that  cannot be resolved by
// the user.
TEST_F(SyncGlobalErrorTest, AuthStateGlobalError) {
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile.get());
  FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
      LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile.get(), BuildMockLoginUIService));
  FakeLoginUI login_ui;
  login_ui_service->SetLoginUI(&login_ui);
  SyncGlobalError error(&service, signin);

  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } table[] = {
    { GoogleServiceAuthError::NONE, false },
    { GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true },
    { GoogleServiceAuthError::USER_NOT_SIGNED_UP, true },
    { GoogleServiceAuthError::CONNECTION_FAILED, false },
    { GoogleServiceAuthError::CAPTCHA_REQUIRED, true },
    { GoogleServiceAuthError::ACCOUNT_DELETED, true },
    { GoogleServiceAuthError::ACCOUNT_DISABLED, true },
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, true },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, true },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED, true },
  };

  for (size_t i = 0; i < sizeof(table)/sizeof(*table); ++i) {
    VerifySyncGlobalErrorResult(&service, login_ui_service, browser(), &error,
                                table[i].error_state, true, table[i].is_error);
    VerifySyncGlobalErrorResult(&service, login_ui_service, browser(), &error,
                                table[i].error_state, false, false);
  }
}

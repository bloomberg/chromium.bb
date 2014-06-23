// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_error_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/test/test_screen.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#endif

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace ash {
namespace test {

namespace {

static const char kTestAccountId[] = "testuser@test.com";

// Notification ID corresponding to kProfileSyncNotificationId + kTestAccountId.
static const std::string kNotificationId =
    "chrome://settings/sync/testuser@test.com";

#if defined(OS_WIN)
class ScreenTypeDelegateDesktop : public gfx::ScreenTypeDelegate {
 public:
  ScreenTypeDelegateDesktop() {}
  virtual ~ScreenTypeDelegateDesktop() {}
  virtual gfx::ScreenType GetScreenTypeForNativeView(
      gfx::NativeView view) OVERRIDE {
    return chrome::IsNativeViewInAsh(view) ?
        gfx::SCREEN_TYPE_ALTERNATE :
        gfx::SCREEN_TYPE_NATIVE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTypeDelegateDesktop);
};
#endif

class FakeLoginUIService: public LoginUIService {
 public:
  FakeLoginUIService() : LoginUIService(NULL) {}
  virtual ~FakeLoginUIService() {}
};

class FakeLoginUI : public LoginUIService::LoginUI {
 public:
  FakeLoginUI() : focus_ui_call_count_(0) {}

  virtual ~FakeLoginUI() {}

  int focus_ui_call_count() const { return focus_ui_call_count_; }

 private:
  // LoginUIService::LoginUI:
  virtual void FocusUI() OVERRIDE {
    ++focus_ui_call_count_;
  }
  virtual void CloseUI() OVERRIDE {}

  int focus_ui_call_count_;
};

KeyedService* BuildMockLoginUIService(
    content::BrowserContext* profile) {
  return new FakeLoginUIService();
}

class SyncErrorNotifierTest : public AshTestBase  {
 public:
  SyncErrorNotifierTest() {}
  virtual ~SyncErrorNotifierTest() {}

  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(kTestAccountId);

    TestingBrowserProcess::GetGlobal();
    AshTestBase::SetUp();

    // Set up a desktop screen for Windows to hold native widgets, used when
    // adding desktop widgets (i.e., message center notifications).
#if defined(OS_WIN)
    test_screen_.reset(aura::TestScreen::Create(gfx::Size()));
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
    gfx::Screen::SetScreenTypeDelegate(new ScreenTypeDelegateDesktop);
#endif

    service_.reset(new NiceMock<ProfileSyncServiceMock>(profile_));

    FakeLoginUIService* login_ui_service = static_cast<FakeLoginUIService*>(
        LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_, BuildMockLoginUIService));
    login_ui_service->SetLoginUI(&login_ui_);

    error_controller_.reset(new SyncErrorController(service_.get()));
    error_notifier_.reset(new SyncErrorNotifier(error_controller_.get(),
                                                profile_));

    notification_ui_manager_ = g_browser_process->notification_ui_manager();
  }

  virtual void TearDown() OVERRIDE {
    error_notifier_->Shutdown();
    service_.reset();
#if defined(OS_WIN)
    test_screen_.reset();
#endif
    profile_manager_.reset();

    AshTestBase::TearDown();
  }

 protected:
  // Utility function to test that SyncErrorNotifier behaves correctly for the
  // given error condition.
  void VerifySyncErrorNotifierResult(GoogleServiceAuthError::State error_state,
                                     bool is_signed_in,
                                     bool is_error) {
    EXPECT_CALL(*service_, HasSyncSetupCompleted())
                .WillRepeatedly(Return(is_signed_in));

    GoogleServiceAuthError auth_error(error_state);
    EXPECT_CALL(*service_, GetAuthError()).WillRepeatedly(
        ReturnRef(auth_error));

    error_controller_->OnStateChanged();
    EXPECT_EQ(is_error, error_controller_->HasError());

    // If there is an error we should see a notification.
    const Notification* notification = notification_ui_manager_->
        FindById(kNotificationId);
    if (is_error) {
      ASSERT_TRUE(notification);
      ASSERT_FALSE(notification->title().empty());
      ASSERT_FALSE(notification->title().empty());
      ASSERT_EQ((size_t)1, notification->buttons().size());
    } else {
      ASSERT_FALSE(notification);
    }
  }

#if defined(OS_WIN)
  scoped_ptr<gfx::Screen> test_screen_;
#endif
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<SyncErrorController> error_controller_;
  scoped_ptr<SyncErrorNotifier> error_notifier_;
  scoped_ptr<NiceMock<ProfileSyncServiceMock> > service_;
  TestingProfile* profile_;
  FakeLoginUI login_ui_;
  NotificationUIManager* notification_ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifierTest);
};

} // namespace

// Test that SyncErrorNotifier shows an notification if a passphrase is
// required.
// Disabled on Windows: http://crbug.com/373238
#if defined(OS_WIN)
#define MAYBE_PassphraseNotification DISABLED_PassphraseNotification
#else
#define MAYBE_PassphraseNotification PassphraseNotification
#endif
TEST_F(SyncErrorNotifierTest, MAYBE_PassphraseNotification) {
  ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));

  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(*service_, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(*service_, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  {
    SCOPED_TRACE("Expected a notification for passphrase error");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */,
                                  true /* error */);
  }

  // Check that no notification is shown if there is no error.
  EXPECT_CALL(*service_, IsPassphraseRequired())
              .WillRepeatedly(Return(false));
  EXPECT_CALL(*service_, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(false));
  {
    SCOPED_TRACE("Not expecting notification since no error exists");
    VerifySyncErrorNotifierResult(GoogleServiceAuthError::NONE,
                                  true /* signed in */,
                                  false /* no error */);
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
        false /* not signed in */,
        false /* no error */);
  }
}

}  // namespace test
}  // namespace ash

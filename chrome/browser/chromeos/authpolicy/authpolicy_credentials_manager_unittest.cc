// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/authpolicy_credentials_manager.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";
constexpr char kProfileEmail[] = "user@example.com";
constexpr char kDisplayName[] = "DisplayName";
constexpr char kGivenName[] = "Given Name";

MATCHER_P(UserAccountDataEq, value, "Compares two UserAccountData") {
  const user_manager::UserManager::UserAccountData& expected_data = value;
  return expected_data.display_name() == arg.display_name() &&
         expected_data.given_name() == arg.given_name() &&
         expected_data.locale() == arg.locale();
}

}  // namespace

class AuthPolicyCredentialsManagerTest : public testing::Test {
 public:
  AuthPolicyCredentialsManagerTest()
      : user_manager_enabler_(std::make_unique<MockUserManager>()),
        local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~AuthPolicyCredentialsManagerTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
    AuthPolicyClient::InitializeFake();
    fake_authpolicy_client()->DisableOperationDelayForTesting();

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileEmail);
    account_id_ =
        AccountId::AdFromUserEmailObjGuid(kProfileEmail, "1234567890");
    mock_user_manager()->AddUser(account_id_);

    base::RunLoop run_loop;
    fake_authpolicy_client()->set_on_get_status_closure(run_loop.QuitClosure());

    profile_ = profile_builder.Build();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    authpolicy_credentials_manager_ =
        static_cast<AuthPolicyCredentialsManager*>(
            AuthPolicyCredentialsManagerFactory::GetInstance()
                ->GetServiceForBrowserContext(profile(), false /* create */));
    EXPECT_TRUE(authpolicy_credentials_manager_);

    EXPECT_CALL(*mock_user_manager(),
                SaveForceOnlineSignin(account_id(), false));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(mock_user_manager());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_user_manager(), Shutdown());
    profile_.reset();
    AuthPolicyClient::Shutdown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  AccountId& account_id() { return account_id_; }
  TestingProfile* profile() { return profile_.get(); }
  AuthPolicyCredentialsManager* authpolicy_credentials_manager() {
    return authpolicy_credentials_manager_;
  }
  chromeos::FakeAuthPolicyClient* fake_authpolicy_client() const {
    return chromeos::FakeAuthPolicyClient::Get();
  }

  MockUserManager* mock_user_manager() {
    return static_cast<MockUserManager*>(user_manager::UserManager::Get());
  }

  int GetNumberOfNotifications() {
    return display_service_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

  void CancelNotificationById(int message_id) {
    const std::string notification_id = kProfileSigninNotificationId +
                                        profile()->GetProfileUserName() +
                                        base::NumberToString(message_id);
    EXPECT_TRUE(display_service_->GetNotification(notification_id));
    display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                         notification_id, false);
  }

  void CallGetUserStatusAndWait() {
    base::RunLoop run_loop;
    fake_authpolicy_client()->set_on_get_status_closure(run_loop.QuitClosure());
    authpolicy_credentials_manager()->GetUserStatus();
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(mock_user_manager());
  }

  content::BrowserTaskEnvironment task_environment_;
  AccountId account_id_;
  std::unique_ptr<TestingProfile> profile_;

  // Owned by AuthPolicyCredentialsManagerFactory.
  AuthPolicyCredentialsManager* authpolicy_credentials_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  ScopedTestingLocalState local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthPolicyCredentialsManagerTest);
};

// Tests saving display and given name into user manager. No error means no
// notifications are shown.
TEST_F(AuthPolicyCredentialsManagerTest, SaveNames) {
  fake_authpolicy_client()->set_display_name(kDisplayName);
  fake_authpolicy_client()->set_given_name(kGivenName);
  user_manager::UserManager::UserAccountData user_account_data(
      base::UTF8ToUTF16(kDisplayName), base::UTF8ToUTF16(kGivenName),
      std::string() /* locale */);

  EXPECT_CALL(*mock_user_manager(),
              UpdateUserAccountData(
                  account_id(),
                  UserAccountDataEq(::testing::ByRef(user_account_data))));
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), false));
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests notification is shown at most once for the same error.
TEST_F(AuthPolicyCredentialsManagerTest, ShowSameNotificationOnce) {
  // In case of expired password save to force online signin and show
  // notification.
  fake_authpolicy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_EXPIRED);
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(1, GetNumberOfNotifications());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_EXPIRED);

  // Do not show the same notification twice.
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests both notifications are shown if different errors occurs.
TEST_F(AuthPolicyCredentialsManagerTest, ShowDifferentNotifications) {
  // In case of expired password save to force online signin and show
  // notification.
  fake_authpolicy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_CHANGED);
  fake_authpolicy_client()->set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED);
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(2, GetNumberOfNotifications());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_CHANGED);
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN);
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests invalid TGT status does not force online signin but still shows
// a notification.
TEST_F(AuthPolicyCredentialsManagerTest, InvalidTGTDoesntForceOnlineSignin) {
  fake_authpolicy_client()->set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED);
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), false));
  CallGetUserStatusAndWait();
  EXPECT_EQ(1, GetNumberOfNotifications());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN);
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests successfull case does not show any notification and does not force
// online signin.
TEST_F(AuthPolicyCredentialsManagerTest, Success_NoNotifications) {
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), false));
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
}

}  // namespace chromeos

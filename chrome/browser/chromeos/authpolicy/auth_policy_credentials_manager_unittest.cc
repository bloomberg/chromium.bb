// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/auth_policy_credentials_manager.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "chromeos/network/network_handler.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kProfileSigninNotificationId[] = "chrome://settings/signin/";
const char kDisplayName[] = "DisplayName";
const char kGivenName[] = "Given Name";

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
      : user_manager_enabler_(new MockUserManager()) {}
  ~AuthPolicyCredentialsManagerTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
    fake_auth_policy_client()->set_operation_delay(
        base::TimeDelta::FromSeconds(0));

    TestingBrowserProcess::GetGlobal()->SetNotificationUIManager(
        base::MakeUnique<StubNotificationUIManager>());

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_ = profile_builder.Build();
    account_id_ = AccountId::AdFromUserEmailObjGuid(
        profile()->GetProfileUserName(), "1234567890");
    mock_user_manager()->AddUser(account_id_);

    base::RunLoop run_loop;
    fake_auth_policy_client()->set_on_get_status_closure(
        run_loop.QuitClosure());

    auth_policy_credentials_manager_ = static_cast<
        AuthPolicyCredentialsManager*>(
        AuthPolicyCredentialsManagerFactory::BuildForProfileIfActiveDirectory(
            profile()));

    EXPECT_CALL(*mock_user_manager(),
                SaveForceOnlineSignin(account_id(), false));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(mock_user_manager());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_user_manager(), Shutdown());
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  AccountId& account_id() { return account_id_; }
  TestingProfile* profile() { return profile_.get(); }
  AuthPolicyCredentialsManager* auth_policy_credentials_manager() {
    return auth_policy_credentials_manager_;
  }
  chromeos::FakeAuthPolicyClient* fake_auth_policy_client() const {
    return static_cast<chromeos::FakeAuthPolicyClient*>(
        chromeos::DBusThreadManager::Get()->GetAuthPolicyClient());
  }

  MockUserManager* mock_user_manager() {
    return static_cast<MockUserManager*>(user_manager::UserManager::Get());
  }

  int GetNumberOfNotifications() {
    return TestingBrowserProcess::GetGlobal()
        ->notification_ui_manager()
        ->GetAllIdsByProfile(profile())
        .size();
  }

  bool CancelNotificationById(int message_id) {
    const std::string notification_id = kProfileSigninNotificationId +
                                        profile()->GetProfileUserName() +
                                        std::to_string(message_id);
    return TestingBrowserProcess::GetGlobal()
        ->notification_ui_manager()
        ->CancelById(notification_id, profile());
  }

  void CallGetUserStatusAndWait() {
    base::RunLoop run_loop;
    fake_auth_policy_client()->set_on_get_status_closure(
        run_loop.QuitClosure());
    auth_policy_credentials_manager()->GetUserStatus();
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(mock_user_manager());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  AccountId account_id_;
  std::unique_ptr<TestingProfile> profile_;

  // Owned by AuthPolicyCredentialsManagerFactory.
  AuthPolicyCredentialsManager* auth_policy_credentials_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(AuthPolicyCredentialsManagerTest);
};

// Tests saving display and given name into user manager. No error means no
// notifications are shown.
TEST_F(AuthPolicyCredentialsManagerTest, SaveNames) {
  fake_auth_policy_client()->set_display_name(kDisplayName);
  fake_auth_policy_client()->set_given_name(kGivenName);
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
  fake_auth_policy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_EXPIRED);
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(1, GetNumberOfNotifications());
  EXPECT_TRUE(CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_EXPIRED));

  // Do not show the same notification twice.
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests both notifications are shown if different errors occurs.
TEST_F(AuthPolicyCredentialsManagerTest, ShowDifferentNotifications) {
  // In case of expired password save to force online signin and show
  // notification.
  fake_auth_policy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_CHANGED);
  fake_auth_policy_client()->set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED);
  EXPECT_CALL(*mock_user_manager(), SaveForceOnlineSignin(account_id(), true));
  CallGetUserStatusAndWait();
  EXPECT_EQ(2, GetNumberOfNotifications());
  EXPECT_TRUE(CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_CHANGED));
  EXPECT_TRUE(CancelNotificationById(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN));
  EXPECT_EQ(0, GetNumberOfNotifications());
}

}  // namespace chromeos

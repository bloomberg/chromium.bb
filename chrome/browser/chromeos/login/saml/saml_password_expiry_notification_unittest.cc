// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using message_center::Notification;

namespace chromeos {

namespace {

constexpr base::TimeDelta kStartTime = base::TimeDelta::FromMilliseconds(1);

constexpr base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kAdvanceWarningTime = base::TimeDelta::FromDays(14);
constexpr base::TimeDelta kOneYear = base::TimeDelta::FromDays(365);
constexpr base::TimeDelta kTenYears = base::TimeDelta::FromDays(10 * 365);

inline base::string16 utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

class SamlPasswordExpiryNotificationTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance time a little bit so that Time::Now().is_null() becomes false.
    test_environment_.FastForwardBy(kStartTime);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test");
    profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                     true);
    profile_->GetPrefs()->SetInteger(
        prefs::kSamlPasswordExpirationAdvanceWarningDays,
        kAdvanceWarningTime.InDays());

    std::unique_ptr<FakeChromeUserManager> fake_user_manager =
        std::make_unique<FakeChromeUserManager>();
    fake_user_manager->AddUser(user_manager::StubAccountId());
    fake_user_manager->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(fake_user_manager->GetPrimaryUser());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void TearDown() override {
    display_service_tester_.reset();
    ResetSamlPasswordExpiryNotificationForTesting();
  }

 protected:
  base::Optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  void SetExpirationTime(base::Time expiration_time) {
    SamlPasswordAttributes attrs(base::Time(), expiration_time, "");
    attrs.SaveToPrefs(profile_->GetPrefs());
  }

  void ExpectNotificationAndDismiss() {
    EXPECT_TRUE(Notification().has_value());
    DismissSamlPasswordExpiryNotification(profile_);
    EXPECT_FALSE(Notification().has_value());
  }

  content::TestBrowserThreadBundle test_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

}  // namespace

TEST_F(SamlPasswordExpiryNotificationTest, ShowAlreadyExpired) {
  ShowSamlPasswordExpiryNotification(profile_, 0);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password has expired"), Notification()->title());
  EXPECT_EQ(utf16("Your current password has expired!\n"
                  "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, ShowWillSoonExpire) {
  ShowSamlPasswordExpiryNotification(profile_, 14);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());
  EXPECT_EQ(utf16("Your current password will expire in less than 14 days!\n"
                  "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_PolicyDisabled) {
  SetExpirationTime(base::Time::Now());
  profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                   false);
  MaybeShowSamlPasswordExpiryNotification(profile_);

  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillNotExpire) {
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  MaybeShowSamlPasswordExpiryNotification(profile_);

  EXPECT_FALSE(Notification().has_value());
  // No notification shown now and nothing shown in the next 10 years.
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_AlreadyExpired) {
  SetExpirationTime(base::Time::Now() - kOneYear);  // Expired last year.
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is shown immediately since password has expired.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password has expired"), Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillSoonExpire) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillEventuallyExpire) {
  SetExpirationTime(base::Time::Now() + kOneYear + (kAdvanceWarningTime / 2));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown when expiration is still over a year away.
  EXPECT_FALSE(Notification().has_value());

  // But, it will be shown once we are in the advance warning window:
  test_environment_.FastForwardBy(kOneYear);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_DeleteExpirationTime) {
  SetExpirationTime(base::Time::Now() + kOneYear);
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Since expiration time is now removed, it is not shown later either.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_PasswordChanged) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());

  // Password is changed and notification is dismissed.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  DismissSamlPasswordExpiryNotification(profile_);

  // From now on, notification will not be reshown.
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_Idempotent) {
  SetExpirationTime(base::Time::Now() + kOneYear);

  // Calling MaybeShowSamlPasswordExpiryNotification should only add one task -
  // to maybe show the notification in about a year.
  int baseline_task_count = test_environment_.GetPendingMainThreadTaskCount();
  MaybeShowSamlPasswordExpiryNotification(profile_);
  int new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);

  // Calling it many times shouldn't create more tasks - we only need one task
  // to show the notification in about a year.
  for (int i = 0; i < 10; i++) {
    MaybeShowSamlPasswordExpiryNotification(profile_);
  }
  new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);
}

TEST_F(SamlPasswordExpiryNotificationTest, TimePasses_NoUserActionTaken) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime +
                    (kOneDay / 2));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // After one year, we are still not quite inside the advance warning window.
  test_environment_.FastForwardBy(kOneYear);
  EXPECT_FALSE(Notification().has_value());

  // But the next day, the notification is shown.
  test_environment_.FastForwardBy(kOneDay);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());
  EXPECT_EQ(utf16("Your current password will expire in less than 14 days!\n"
                  "Click here to choose a new password"),
            Notification()->message());

  // As time passes, the notification updates each day.
  test_environment_.FastForwardBy(kAdvanceWarningTime / 2);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password will soon expire"), Notification()->title());
  EXPECT_EQ(utf16("Your current password will expire in less than 7 days!\n"
                  "Click here to choose a new password"),
            Notification()->message());

  test_environment_.FastForwardBy(kAdvanceWarningTime / 2);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password has expired"), Notification()->title());
  EXPECT_EQ(utf16("Your current password has expired!\n"
                  "Click here to choose a new password"),
            Notification()->message());

  test_environment_.FastForwardBy(kOneYear);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password has expired"), Notification()->title());
  EXPECT_EQ(utf16("Your current password has expired!\n"
                  "Click here to choose a new password"),
            Notification()->message());
}

TEST_F(SamlPasswordExpiryNotificationTest, TimePasses_NotificationDismissed) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime / 2);
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Notification appears once we are inside the advance warning window.
  test_environment_.FastForwardBy(kOneYear);
  ExpectNotificationAndDismiss();

  // If a day goes past and the password still hasn't been changed, then the
  // notification will be shown again.
  test_environment_.FastForwardBy(kOneDay);
  ExpectNotificationAndDismiss();

  // This continues each day even once the password has long expired.
  test_environment_.FastForwardBy(kTenYears);
  ExpectNotificationAndDismiss();
  test_environment_.FastForwardBy(kOneDay);
  ExpectNotificationAndDismiss();
}

}  // namespace chromeos

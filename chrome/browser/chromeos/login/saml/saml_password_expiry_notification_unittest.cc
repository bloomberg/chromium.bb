// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using message_center::Notification;

namespace chromeos {

constexpr base::TimeDelta kTimeAdvance = base::TimeDelta::FromMilliseconds(1);

class SamlPasswordExpiryNotificationTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance time a little bit so that Time::Now().is_null() becomes false.
    test_environment_.FastForwardBy(kTimeAdvance);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test");

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void TearDown() override { display_service_tester_.reset(); }

 protected:
  base::Optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  void SetExpirationTime(base::Time expiration_time) {
    SamlPasswordAttributes attrs(base::Time(), expiration_time, "");
    attrs.SaveToPrefs(profile_->GetPrefs());
  }

  content::TestBrowserThreadBundle test_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(SamlPasswordExpiryNotificationTest, ShowAlreadyExpired) {
  ShowSamlPasswordExpiryNotification(profile_, 0);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(base::ASCIIToUTF16("Password has expired"),
            Notification()->title());
  EXPECT_EQ(base::ASCIIToUTF16("Your current password has expired!\n"
                               "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, ShowWillSoonExpire) {
  ShowSamlPasswordExpiryNotification(profile_, 14);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(base::ASCIIToUTF16("Password will soon expire"),
            Notification()->title());
  EXPECT_EQ(base::ASCIIToUTF16(
                "Your current password will expire in less than 14 days!\n"
                "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillNotExpire) {
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  MaybeShowSamlPasswordExpiryNotification(profile_);

  EXPECT_FALSE(Notification().has_value());
  // No notification shown now and nothing shown in the next 10,000 days.
  test_environment_.FastForwardBy(base::TimeDelta::FromDays(10000));
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_AlreadyExpired) {
  SetExpirationTime(base::Time::Now() - base::TimeDelta::FromDays(30));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is shown immediately since password has expired.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(base::ASCIIToUTF16("Password has expired"),
            Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillSoonExpire) {
  SetExpirationTime(base::Time::Now() + base::TimeDelta::FromDays(5));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(base::ASCIIToUTF16("Password will soon expire"),
            Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_WillEventuallyExpire) {
  SetExpirationTime(base::Time::Now() + base::TimeDelta::FromDays(95));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // But, it will be shown eventually.
  test_environment_.FastForwardBy(base::TimeDelta::FromDays(90));
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(base::ASCIIToUTF16("Password will soon expire"),
            Notification()->title());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_DeleteExpirationTime) {
  SetExpirationTime(base::Time::Now() + base::TimeDelta::FromDays(95));
  MaybeShowSamlPasswordExpiryNotification(profile_);

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Since expiration time is now removed, it is not shown later either.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  test_environment_.FastForwardBy(base::TimeDelta::FromDays(90));
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, MaybeShow_Idempotent) {
  SetExpirationTime(base::Time::Now() + base::TimeDelta::FromDays(95));

  // Calling MaybeShowSamlPasswordExpiryNotification should only add one task -
  // to maybe show the notification in about 90 days.
  int baseline_task_count = test_environment_.GetPendingMainThreadTaskCount();
  MaybeShowSamlPasswordExpiryNotification(profile_);
  int new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);

  // Calling it many times shouldn't create more tasks - we only need one task
  // to show the notification in about 90 days.
  for (int i = 0; i < 10; i++) {
    MaybeShowSamlPasswordExpiryNotification(profile_);
  }
  new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);
}

}  // namespace chromeos

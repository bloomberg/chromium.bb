// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using message_center::Notification;

namespace chromeos {

class SamlPasswordExpiryNotificationTest : public testing::Test {
 public:
  void SetUp() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);
  }

  void TearDown() override { display_service_tester_.reset(); }

 protected:
  base::Optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(SamlPasswordExpiryNotificationTest, ShowAlreadyExpired) {
  ShowSamlPasswordExpiryNotification(&profile_, 0);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(base::ASCIIToUTF16("Password has expired"),
            Notification()->title());
  EXPECT_EQ(base::ASCIIToUTF16("Your current password has expired!\n"
                               "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, ShowWillSoonExpire) {
  ShowSamlPasswordExpiryNotification(&profile_, 14);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(base::ASCIIToUTF16("Password will soon expire"),
            Notification()->title());
  EXPECT_EQ(base::ASCIIToUTF16(
                "Your current password will expire in less than 14 days!\n"
                "Click here to choose a new password"),
            Notification()->message());

  DismissSamlPasswordExpiryNotification(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

}  // namespace chromeos

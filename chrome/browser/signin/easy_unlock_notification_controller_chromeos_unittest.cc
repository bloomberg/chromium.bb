// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_notification_controller_chromeos.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace {

const char kPhoneName[] = "Nexus 6";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() : message_center::FakeMessageCenter() {}
  ~TestMessageCenter() override {}

  // message_center::FakeMessageCenter:
  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    auto iter = std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        });
    return iter != notifications_.end() ? iter->get() : nullptr;
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_.push_back(std::move(notification));
  }

  void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<message_center::Notification> new_notification) override {
    RemoveNotification(old_id, false /* by_user */);
    AddNotification(std::move(new_notification));
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    if (!FindVisibleNotificationById(id))
      return;

    notifications_.erase(std::find_if(
        notifications_.begin(), notifications_.end(),
        [id](const std::shared_ptr<message_center::Notification> notification) {
          return notification->id() == id;
        }));
  }

 private:
  std::vector<std::shared_ptr<message_center::Notification>> notifications_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenter);
};

class TestableNotificationController
    : public EasyUnlockNotificationControllerChromeOS {
 public:
  TestableNotificationController(Profile* profile,
                                 message_center::MessageCenter* message_center)
      : EasyUnlockNotificationControllerChromeOS(profile, message_center) {}

  ~TestableNotificationController() override {}

  // EasyUnlockNotificationControllerChromeOS:
  MOCK_METHOD0(LaunchEasyUnlockSettings, void());
  MOCK_METHOD0(LockScreen, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestableNotificationController);
};

}  // namespace

class EasyUnlockNotificationControllerChromeOSTest : public ::testing::Test {
 protected:
  EasyUnlockNotificationControllerChromeOSTest()
      : notification_controller_(&profile_, &message_center_) {}

  ~EasyUnlockNotificationControllerChromeOSTest() override {}

  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestMessageCenter message_center_;
  testing::StrictMock<TestableNotificationController> notification_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockNotificationControllerChromeOSTest);
};

TEST_F(EasyUnlockNotificationControllerChromeOSTest,
       TestShowChromebookAddedNotification) {
  const char kNotificationId[] = "easyunlock_notification_ids.chromebook_added";

  notification_controller_.ShowChromebookAddedNotification();
  message_center::Notification* notification =
      message_center_.FindVisibleNotificationById(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking notification button should launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->ButtonClick(0);

  // Clicking the notification itself should also launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->Click();
}

TEST_F(EasyUnlockNotificationControllerChromeOSTest,
       TestShowPairingChangeNotification) {
  const char kNotificationId[] = "easyunlock_notification_ids.pairing_change";

  notification_controller_.ShowPairingChangeNotification();
  message_center::Notification* notification =
      message_center_.FindVisibleNotificationById(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(2u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking 1st notification button should lock screen settings.
  EXPECT_CALL(notification_controller_, LockScreen());
  notification->ButtonClick(0);

  // Clicking 2nd notification button should launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->ButtonClick(1);

  // Clicking the notification itself should do nothing.
  notification->Click();
}

TEST_F(EasyUnlockNotificationControllerChromeOSTest,
       TestShowPairingChangeAppliedNotification) {
  const char kNotificationId[] =
      "easyunlock_notification_ids.pairing_change_applied";

  notification_controller_.ShowPairingChangeAppliedNotification(kPhoneName);
  message_center::Notification* notification =
      message_center_.FindVisibleNotificationById(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Check that the phone name is in the notification message.
  EXPECT_NE(std::string::npos,
            notification->message().find(base::UTF8ToUTF16(kPhoneName)));

  // Clicking notification button should launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->ButtonClick(0);

  // Clicking the notification itself should also launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->Click();
}

TEST_F(EasyUnlockNotificationControllerChromeOSTest,
       PairingAppliedRemovesPairingChange) {
  const char kPairingChangeId[] = "easyunlock_notification_ids.pairing_change";
  const char kPairingAppliedId[] =
      "easyunlock_notification_ids.pairing_change_applied";

  notification_controller_.ShowPairingChangeNotification();
  EXPECT_TRUE(message_center_.FindVisibleNotificationById(kPairingChangeId));

  notification_controller_.ShowPairingChangeAppliedNotification(kPhoneName);
  EXPECT_FALSE(message_center_.FindVisibleNotificationById(kPairingChangeId));
  EXPECT_TRUE(message_center_.FindVisibleNotificationById(kPairingAppliedId));
}

TEST_F(EasyUnlockNotificationControllerChromeOSTest,
       TestShowPromotionNotification) {
  const char kNotificationId[] = "easyunlock_notification_ids.promotion";

  notification_controller_.ShowPromotionNotification();
  message_center::Notification* notification =
      message_center_.FindVisibleNotificationById(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking notification button should launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->ButtonClick(0);

  // Clicking the notification itself should also launch settings.
  EXPECT_CALL(notification_controller_, LaunchEasyUnlockSettings());
  notification->Click();
}

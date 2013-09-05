// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/desktop_notifications.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrEq;

namespace file_manager {

namespace {

class MockFileManagerNotificationsOnMount : public DesktopNotifications {
 public:
  explicit MockFileManagerNotificationsOnMount(Profile* profile)
      : DesktopNotifications(profile) {
  }

  virtual ~MockFileManagerNotificationsOnMount() {}

  MOCK_METHOD3(ShowNotificationWithMessage,
               void(NotificationType, const std::string&, const string16&));
  MOCK_METHOD2(HideNotification, void(NotificationType, const std::string&));
};

MATCHER_P2(String16Equals, id, label, "") {
  return arg == l10n_util::GetStringFUTF16(id, UTF8ToUTF16(label));
}

}  // namespace

TEST(FileManagerMountNotificationsTest, GoodDevice) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, true, false);
};

TEST(FileManagerMountNotificationsTest, GoodDeviceWithBadParent) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));

  {
    InSequence s;

    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path), _));
    EXPECT_CALL(*mocked_notifications, HideNotification(
        DesktopNotifications::DEVICE_FAIL,
        StrEq(notification_path)));
  }

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
}

TEST(FileManagerMountNotificationsTest, UnsupportedDevice) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
      String16Equals(IDS_DEVICE_UNSUPPORTED_MESSAGE, device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileManagerMountNotificationsTest, UnsupportedWithUnknownParent) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));

  {
    InSequence s;

    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path), _));
    EXPECT_CALL(*mocked_notifications, HideNotification(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path)));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
        String16Equals(IDS_DEVICE_UNSUPPORTED_MESSAGE,
        device_label)));
  }

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileManagerMountNotificationsTest, MountPartialSuccess) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
          String16Equals(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
          device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileManagerMountNotificationsTest, Unknown) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
      String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
}

TEST(FileManagerMountNotificationsTest, NonASCIILabel) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  // "RA (U+30E9) BE (U+30D9) RU (U+30EB)" in Katakana letters.
  std::string device_label("\xE3\x83\xA9\xE3\x83\x99\xE3\x83\xAB");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
      String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
}

TEST(FileManagerMountNotificationsTest, MulitpleFail) {
  MockFileManagerNotificationsOnMount* mocked_notifications =
      new MockFileManagerNotificationsOnMount(NULL);
  scoped_ptr<DesktopNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              DesktopNotifications::DEVICE, StrEq(notification_path)));
  {
    InSequence s;
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
        String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)))
        .RetiresOnSaturation();
    EXPECT_CALL(*mocked_notifications, HideNotification(
        DesktopNotifications::DEVICE_FAIL, notification_path));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
        String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)));
    EXPECT_CALL(*mocked_notifications, HideNotification(
        DesktopNotifications::DEVICE_FAIL, notification_path));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        DesktopNotifications::DEVICE_FAIL, StrEq(notification_path),
        String16Equals(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
                       device_label)));
  }

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
}

}  // namespace file_manager.

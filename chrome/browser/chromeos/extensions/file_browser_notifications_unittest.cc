// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::AnyNumber;

namespace chromeos {

class MockFileBrowserNotificationsOnMount : public FileBrowserNotifications {
 public:
  explicit MockFileBrowserNotificationsOnMount(Profile* profile)
      : FileBrowserNotifications(profile) {
  }

  virtual ~MockFileBrowserNotificationsOnMount() {}

  MOCK_METHOD3(ShowNotificationWithMessage, void(NotificationType,
      const std::string&, const string16&));
  MOCK_METHOD2(HideNotification, void(NotificationType, const std::string&));
};

MATCHER_P2(String16Equals, id, label, "") {
  return arg == l10n_util::GetStringFUTF16(id, ASCIIToUTF16(label));
}

TEST(FileBrowserMountNotificationsTest, GoodDevice) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, true, false);
};

TEST(FileBrowserMountNotificationsTest, GoodDeviceWithBadParent) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));

  {
    InSequence s;

    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path), _));
    EXPECT_CALL(*mocked_notifications, HideNotification(
        FileBrowserNotifications::DEVICE_FAIL,
        StrEq(notification_path)));
  }

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
}

TEST(FileBrowserMountNotificationsTest, UnsupportedDevice) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
      String16Equals(IDS_DEVICE_UNSUPPORTED_MESSAGE, device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileBrowserMountNotificationsTest, UnsupportedWithUnknownParent) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);

  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));

  {
    InSequence s;

    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path), _));
    EXPECT_CALL(*mocked_notifications, HideNotification(
        FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path)));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
        String16Equals(IDS_DEVICE_UNSUPPORTED_MESSAGE,
        device_label)));
  }

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, true, false, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileBrowserMountNotificationsTest, MountPartialSuccess) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
          String16Equals(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
          device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, true, false);
  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, true);
}

TEST(FileBrowserMountNotificationsTest, Unknown) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));
  EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
      FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
      String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)));

  notifications->ManageNotificationsOnMountCompleted(notification_path,
      device_label, false, false, false);
}

TEST(FileBrowserMountNotificationsTest, MulitpleFail) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications->RegisterDevice(notification_path);
  EXPECT_CALL(*mocked_notifications, HideNotification(
              FileBrowserNotifications::DEVICE, StrEq(notification_path)));
  {
    InSequence s;
      EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
          FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
              String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)))
          .RetiresOnSaturation();
      EXPECT_CALL(*mocked_notifications, HideNotification(
          FileBrowserNotifications::DEVICE_FAIL, notification_path));
      EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
          FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
              String16Equals(IDS_DEVICE_UNKNOWN_MESSAGE, device_label)));
      EXPECT_CALL(*mocked_notifications, HideNotification(
          FileBrowserNotifications::DEVICE_FAIL, notification_path));
      EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
          FileBrowserNotifications::DEVICE_FAIL, StrEq(notification_path),
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

TEST(FileBrowserGDataNotificationsTest, GDataSyncSuccess) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC, _)).Times(AnyNumber());
  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC_SUCCESS, _)).Times(AnyNumber());
  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC_FAIL, _)).Times(AnyNumber());
  {
    InSequence s;
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC, _,
        String16Equals(IDS_CHROMEOS_GDATA_SYNC_PROGRESS_MESSAGE, "2")));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC, _,
        String16Equals(IDS_CHROMEOS_GDATA_SYNC_PROGRESS_MESSAGE, "1")));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC_SUCCESS, _,
        l10n_util::GetStringUTF16(
            IDS_CHROMEOS_GDATA_SYNC_FINISHED_SUCCESS_MESSAGE)));
  }

  notifications->ManageNotificationOnGDataSyncProgress(2);
  notifications->ManageNotificationOnGDataSyncProgress(1);
  notifications->ManageNotificationOnGDataSyncFinish(true);
};


TEST(FileBrowserGDataNotificationsTest, GDataSyncFailAndSuccess) {
  MockFileBrowserNotificationsOnMount* mocked_notifications =
      new MockFileBrowserNotificationsOnMount(NULL);
  scoped_ptr<FileBrowserNotifications> notifications(mocked_notifications);

  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC, _)).Times(AnyNumber());
  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC_SUCCESS, _)).Times(AnyNumber());
  EXPECT_CALL(*mocked_notifications, HideNotification(
      FileBrowserNotifications::GDATA_SYNC_FAIL, _)).Times(AnyNumber());
  {
    InSequence s;
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC, _,
        String16Equals(IDS_CHROMEOS_GDATA_SYNC_PROGRESS_MESSAGE, "5")));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC_FAIL, _,
        l10n_util::GetStringUTF16(
            IDS_CHROMEOS_GDATA_SYNC_FINISHED_FAILURE_MESSAGE)));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC, _,
        String16Equals(IDS_CHROMEOS_GDATA_SYNC_PROGRESS_MESSAGE, "2")));
    EXPECT_CALL(*mocked_notifications, ShowNotificationWithMessage(
        FileBrowserNotifications::GDATA_SYNC_SUCCESS, _,
        l10n_util::GetStringUTF16(
            IDS_CHROMEOS_GDATA_SYNC_FINISHED_SUCCESS_MESSAGE)));
  }

  notifications->ManageNotificationOnGDataSyncProgress(5);
  notifications->ManageNotificationOnGDataSyncFinish(false);
  notifications->ManageNotificationOnGDataSyncProgress(2);
  notifications->ManageNotificationOnGDataSyncFinish(true);
};

}  // namespace chromeos.

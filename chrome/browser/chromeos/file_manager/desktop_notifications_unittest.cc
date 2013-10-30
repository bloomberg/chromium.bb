// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/desktop_notifications.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace file_manager {

namespace {

// This class records parameters for functions to show and hide
// notifications.
class RecordedDesktopNotifications : public DesktopNotifications {
 public:
  explicit RecordedDesktopNotifications(Profile* profile)
      : DesktopNotifications(profile) {
  }

  virtual ~RecordedDesktopNotifications() {}

  virtual void ShowNotificationWithMessage(
      NotificationType type,
      const std::string& path,
      const string16& message)  OVERRIDE {
    ShowAndHideParams params;
    params.event = SHOW;
    params.type = type;
    params.path = path;
    params.message = message;
    params_.push_back(params);
  }

  virtual void HideNotification(NotificationType type,
                                const std::string& path) OVERRIDE {
    ShowAndHideParams params;
    params.event = HIDE;
    params.type = type;
    params.path = path;
    params_.push_back(params);
  }

  enum Event {
    SHOW,
    HIDE,
  };

  // Used to record parameters passed to ShowNotificationWithMessage() and
  // HideNotification().
  struct ShowAndHideParams {
    Event event;
    NotificationType type;
    std::string path;
    string16 message;  // Empty for HideNotification().
  };

  // Returns parameters passed to ShowNotificationWithMessage() and
  // HideNotificationParams().
  const std::vector<ShowAndHideParams>& params() const {
    return params_;
  }

 private:
  std::vector<ShowAndHideParams> params_;
};

}  // namespace

TEST(FileManagerMountNotificationsTest, GoodDevice) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      true  /* is_parent */,
      true  /* success */,
      false  /* is_unsupported */);
  // Should hide a DEVICE notification.
  ASSERT_EQ(1U, notifications.params().size());
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
};

TEST(FileManagerMountNotificationsTest, GoodDeviceWithBadParent) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      true  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      true  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(3U, notifications.params().size());
  // Should hide a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[2].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[2].type);
  EXPECT_EQ(notification_path, notifications.params()[2].path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      true  /* success */,
      false  /* is_unsupported */);
  // Should do nothing this time.
  ASSERT_EQ(3U, notifications.params().size());
}

TEST(FileManagerMountNotificationsTest, UnsupportedDevice) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      true  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // And should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNSUPPORTED_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[1].message);
}

TEST(FileManagerMountNotificationsTest, UnsupportedWithUnknownParent) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      true  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // And should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      true  /* is_unsupported */);
  ASSERT_EQ(4U, notifications.params().size());
  // Should hide DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[2].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[2].type);
  EXPECT_EQ(notification_path, notifications.params()[2].path);
  // Should show DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[3].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[3].type);
  EXPECT_EQ(notification_path, notifications.params()[3].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNSUPPORTED_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[3].message);
}

TEST(FileManagerMountNotificationsTest, MountPartialSuccess) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      true  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(1U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      true  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[1].message);
}

TEST(FileManagerMountNotificationsTest, Unknown) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[1].message);
}

TEST(FileManagerMountNotificationsTest, NonASCIILabel) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  // "RA (U+30E9) BE (U+30D9) RU (U+30EB)" in Katakana letters.
  std::string device_label("\xE3\x83\xA9\xE3\x83\x99\xE3\x83\xAB");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  ASSERT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[1].message);
}

TEST(FileManagerMountNotificationsTest, MulitpleFail) {
  RecordedDesktopNotifications notifications(NULL);

  std::string notification_path("system_path_prefix");
  std::string device_label("label");

  notifications.RegisterDevice(notification_path);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      true  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  EXPECT_EQ(2U, notifications.params().size());
  // Should hide DEVICE notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[0].event);
  EXPECT_EQ(DesktopNotifications::DEVICE, notifications.params()[0].type);
  EXPECT_EQ(notification_path, notifications.params()[0].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[1].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[1].type);
  EXPECT_EQ(notification_path, notifications.params()[1].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[1].message);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  EXPECT_EQ(4U, notifications.params().size());
  // Should hide DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[2].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL, notifications.params()[2].type);
  EXPECT_EQ(notification_path, notifications.params()[2].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[3].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[3].type);
  EXPECT_EQ(notification_path, notifications.params()[3].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_DEVICE_UNKNOWN_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[3].message);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  EXPECT_EQ(6U, notifications.params().size());
  // Should hide DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::HIDE,
            notifications.params()[4].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL, notifications.params()[4].type);
  EXPECT_EQ(notification_path, notifications.params()[4].path);
  // Should show a DEVICE_FAIL notification.
  EXPECT_EQ(RecordedDesktopNotifications::SHOW,
            notifications.params()[5].event);
  EXPECT_EQ(DesktopNotifications::DEVICE_FAIL,
            notifications.params()[5].type);
  EXPECT_EQ(notification_path, notifications.params()[5].path);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
                                 UTF8ToUTF16(device_label)),
      notifications.params()[5].message);

  notifications.ManageNotificationsOnMountCompleted(
      notification_path,
      device_label,
      false  /* is_parent */,
      false  /* success */,
      false  /* is_unsupported */);
  EXPECT_EQ(6U, notifications.params().size());
  // Should do nothing this time.
}

}  // namespace file_manager.

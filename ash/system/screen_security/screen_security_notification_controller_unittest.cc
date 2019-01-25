// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_security/screen_security_notification_controller.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "ui/message_center/message_center.h"

namespace ash {

using ScreenSecurityNotificationControllerTest = AshTestBase;

namespace {

message_center::Notification* FindNotification(const std::string& id) {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
}

}  // namespace

TEST_F(ScreenSecurityNotificationControllerTest,
       ShowScreenCaptureNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      base::DoNothing(), base::RepeatingClosure(), base::string16());
  EXPECT_TRUE(FindNotification(kScreenCaptureNotificationId));
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
}

TEST_F(ScreenSecurityNotificationControllerTest, ShowScreenShareNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::DoNothing(), base::string16());
  EXPECT_TRUE(FindNotification(kScreenShareNotificationId));
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();
  EXPECT_FALSE(FindNotification(kScreenShareNotificationId));
}

TEST_F(ScreenSecurityNotificationControllerTest,
       DoNotShowScreenCaptureNotificationWhenCasting) {
  Shell::Get()->OnCastingSessionStartedOrStopped(true /* started */);
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      base::DoNothing(), base::RepeatingClosure(), base::string16());
  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
  Shell::Get()->OnCastingSessionStartedOrStopped(false /* started */);
  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
}

}  // namespace ash

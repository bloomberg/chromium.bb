// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/chromeos/drive/tray_drive_notice.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace internal {

class DriveNoticeTest : public ash::test::AshTestBase {
 public:
  DriveNoticeTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    TrayItemView::DisableAnimationsForTest();

    // Ownership of |notice_tray_item_| passed to system tray.
    notice_tray_item_ = new TrayDriveNotice(GetSystemTray());
    notice_tray_item_->SetTimeVisibleForTest(0);
    GetSystemTray()->AddTrayItem(notice_tray_item_);
  }

  TrayDriveNotice* notice_tray_item() { return notice_tray_item_; }

  SystemTray* GetSystemTray() {
    return Shell::GetInstance()->GetPrimarySystemTray();
  }

  void NotifyOfflineEnabled() {
    Shell::GetInstance()->system_tray_notifier()->NotifyDriveOfflineEnabled();
  }

 private:
  // |notice_tray_item_| is owned by system tray.
  TrayDriveNotice* notice_tray_item_;
};

TEST_F(DriveNoticeTest, NotifyEnabled) {
  // Tray item views should not be visible initially.
  EXPECT_FALSE(notice_tray_item()->GetTrayView()->visible());
  EXPECT_FALSE(notice_tray_item()->default_view());
  EXPECT_FALSE(notice_tray_item()->detailed_view());

  // Tray item view should appear after notifying offline enabled.
  NotifyOfflineEnabled();
  EXPECT_TRUE(notice_tray_item()->GetTrayView()->visible());

  // Open system bubble and check default view is visible.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_TRUE(GetSystemTray()->HasSystemBubble());
  EXPECT_TRUE(notice_tray_item()->default_view());

  // Open detailed view and check detailed view is visible.
  GetSystemTray()->ShowDetailedView(
      notice_tray_item(), 0, false, BUBBLE_USE_EXISTING);
  EXPECT_TRUE(notice_tray_item()->detailed_view());

  GetSystemTray()->CloseSystemBubble();
}

TEST_F(DriveNoticeTest, AutomaticallyHidden) {
  // The tray item should automatically be hidden after a period of time.
  EXPECT_FALSE(notice_tray_item()->GetTrayView()->visible());
  NotifyOfflineEnabled();
  EXPECT_TRUE(notice_tray_item()->GetTrayView()->visible());

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(notice_tray_item()->GetTrayView()->visible());

  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
  EXPECT_FALSE(notice_tray_item()->default_view());
}

}  // internal
}  // ash

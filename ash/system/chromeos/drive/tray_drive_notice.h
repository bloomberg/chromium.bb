// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DRIVE_TRAY_DRIVE_NOTICE_H_
#define ASH_SYSTEM_DRIVE_TRAY_DRIVE_NOTICE_H_

#include "ash/system/drive/drive_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "ash/system/tray/tray_item_more.h"
#include "base/timer/timer.h"

namespace ash {
namespace internal {

class DriveNoticeDetailedView;

// A tray item shown to the user as a notice that Google Drive offline mode has
// been enabled automatically. This tray item is mainly informational and is
// automatically dismissed after a short period of time.
class ASH_EXPORT TrayDriveNotice : public TrayImageItem,
                                   public DriveObserver {
 public:
  explicit TrayDriveNotice(SystemTray* system_tray);
  virtual ~TrayDriveNotice();

  views::View* GetTrayView();
  views::View* default_view() { return default_view_; }
  views::View* detailed_view() { return detailed_view_; }

  // Set the time the tray item is visible after opting-in for testing.
  void SetTimeVisibleForTest(int time_visible_secs);

 private:
  void HideNotice();

  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from DriveObserver.
  virtual void OnDriveJobUpdated(const DriveOperationStatus& status) OVERRIDE;
  virtual void OnDriveOfflineEnabled() OVERRIDE;

  views::View* default_view_;
  views::View* detailed_view_;
  base::OneShotTimer<TrayDriveNotice> visibility_timer_;
  bool showing_item_;
  int time_visible_secs_;

  DISALLOW_COPY_AND_ASSIGN(TrayDriveNotice);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DRIVE_TRAY_DRIVE_NOTICE_H_

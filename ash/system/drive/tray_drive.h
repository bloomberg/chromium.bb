// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_
#define ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_

#include "ash/system/drive/drive_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "base/timer/timer.h"

namespace views {
class Label;
}

namespace ash {
namespace tray {
class DriveTrayView;
class DriveDefaultView;
class DriveDetailedView;
}

class TrayDrive : public TrayImageItem,
                  public DriveObserver {
 public:
  explicit TrayDrive(SystemTray* system_tray);
  virtual ~TrayDrive();

 private:
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

  // Delayed hiding of the tray item after encountering an empty operation list.
  void HideIfNoOperations();

  tray::DriveDefaultView* default_;
  tray::DriveDetailedView* detailed_;
  base::OneShotTimer<TrayDrive> hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(TrayDrive);
};

}  // namespace ash

#endif  // ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_

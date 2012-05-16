// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_
#define ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_
#pragma once

#include "ash/system/drive/drive_observer.h"
#include "ash/system/tray/tray_image_item.h"

namespace views {
class Label;
}

namespace ash {

namespace internal {

namespace tray {
class DriveTrayView;
class DriveDefaultView;
class DriveDetailedView;
}

class TrayDrive : public TrayImageItem,
                  public DriveObserver {
 public:
  TrayDrive();
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
  virtual void OnDriveRefresh(const DriveOperationStatusList& list) OVERRIDE;

  // Delayed re-check of the status after encounter operation depleted list.
  void OnStatusCheck();

  void UpdateTrayIcon(bool show);

  tray::DriveDefaultView* default_;
  tray::DriveDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayDrive);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DRIVE_TRAY_DRIVE_H_

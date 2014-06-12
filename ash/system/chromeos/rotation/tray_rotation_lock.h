// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_
#define ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_

#include "ash/shell_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"

namespace ash {

namespace tray {
class RotationLockDefaultView;
}  // namespace tray

// TrayRotationLock is a provider of views for the SystemTray. Both a tray view
// and a default view are provided. Each view indicates the current state of
// the rotation lock for the display which it appears on. The default view can
// be interacted with, it toggles the state of the rotation lock.
// TrayRotationLock is only available on the primary display.
class ASH_EXPORT TrayRotationLock : public TrayImageItem,
                                    public MaximizeModeController::Observer,
                                    public ShellObserver {
 public:
  explicit TrayRotationLock(SystemTray* system_tray);
  virtual ~TrayRotationLock();

  // MaximizeModeController::Observer:
  virtual void OnRotationLockChanged(bool rotation_locked) OVERRIDE;

  // SystemTrayItem:
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;

  // ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

 protected:
  // TrayImageItem:
  virtual bool GetInitialVisibility() OVERRIDE;

 private:
  friend class TrayRotationLockTest;

  // True if |on_primary_display_|, maximize mode is enabled, and rotation is
  // locked.
  bool ShouldBeVisible();

  // True if this has been created by a SystemTray on the primary display.
  bool on_primary_display_;

  DISALLOW_COPY_AND_ASSIGN(TrayRotationLock);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_ROTATION_TRAY_ROTATION_LOCK_H_

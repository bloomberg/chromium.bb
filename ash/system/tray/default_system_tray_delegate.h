// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class ASH_EXPORT DefaultSystemTrayDelegate : public SystemTrayDelegate {
 public:
  DefaultSystemTrayDelegate();
  ~DefaultSystemTrayDelegate() override;

  // SystemTrayDelegate
  bool GetTrayVisibilityOnStartup() override;
  user::LoginStatus GetUserLoginStatus() const override;
  std::string GetSupervisedUserManager() const override;
  bool IsUserSupervised() const override;
  void GetSystemUpdateInfo(UpdateInfo* info) const override;
  bool ShouldShowSettings() override;
  bool ShouldShowDisplayNotification() override;
  void ToggleBluetooth() override;
  bool IsBluetoothDiscovering() override;
  bool GetBluetoothAvailable() override;
  bool GetBluetoothEnabled() override;
  bool GetBluetoothDiscovering() override;
  VolumeControlDelegate* GetVolumeControlDelegate() const override;
  void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) override;
  int GetSystemTrayMenuWidth() override;

 private:
  bool bluetooth_enabled_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSystemTrayDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_

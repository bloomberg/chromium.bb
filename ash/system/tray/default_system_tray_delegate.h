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
  virtual ~DefaultSystemTrayDelegate();

  // Overridden from SystemTrayDelegate:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE;
  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE;
  virtual void ChangeProfilePicture() OVERRIDE;
  virtual const std::string GetEnterpriseDomain() const OVERRIDE;
  virtual const base::string16 GetEnterpriseMessage() const OVERRIDE;
  virtual const std::string GetSupervisedUserManager() const OVERRIDE;
  virtual const base::string16 GetSupervisedUserManagerName() const
      OVERRIDE;
  virtual const base::string16 GetSupervisedUserMessage() const OVERRIDE;
  virtual bool SystemShouldUpgrade() const OVERRIDE;
  virtual base::HourClockType GetHourClockType() const OVERRIDE;
  virtual void ShowSettings() OVERRIDE;
  virtual bool ShouldShowSettings() OVERRIDE;
  virtual void ShowDateSettings() OVERRIDE;
  virtual void ShowSetTimeDialog() OVERRIDE;
  virtual void ShowNetworkSettings(const std::string& service_path) OVERRIDE;
  virtual void ShowBluetoothSettings() OVERRIDE;
  virtual void ShowDisplaySettings() OVERRIDE;
  virtual void ShowChromeSlow() OVERRIDE;
  virtual bool ShouldShowDisplayNotification() OVERRIDE;
  virtual void ShowDriveSettings() OVERRIDE;
  virtual void ShowIMESettings() OVERRIDE;
  virtual void ShowHelp() OVERRIDE;
  virtual void ShowAccessibilityHelp() OVERRIDE;
  virtual void ShowAccessibilitySettings() OVERRIDE;
  virtual void ShowPublicAccountInfo() OVERRIDE;
  virtual void ShowEnterpriseInfo() OVERRIDE;
  virtual void ShowSupervisedUserInfo() OVERRIDE;
  virtual void ShowUserLogin() OVERRIDE;
  virtual bool ShowSpringChargerReplacementDialog() OVERRIDE;
  virtual bool IsSpringChargerReplacementDialogVisible() OVERRIDE;
  virtual bool HasUserConfirmedSafeSpringCharger() OVERRIDE;
  virtual void ShutDown() OVERRIDE;
  virtual void SignOut() OVERRIDE;
  virtual void RequestLockScreen() OVERRIDE;
  virtual void RequestRestartForUpdate() OVERRIDE;
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* list) OVERRIDE;
  virtual void BluetoothStartDiscovering() OVERRIDE;
  virtual void BluetoothStopDiscovering() OVERRIDE;
  virtual void ConnectToBluetoothDevice(const std::string& address) OVERRIDE;
  virtual void GetCurrentIME(IMEInfo* info) OVERRIDE;
  virtual void GetAvailableIMEList(IMEInfoList* list) OVERRIDE;
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) OVERRIDE;
  virtual void SwitchIME(const std::string& ime_id) OVERRIDE;
  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE;
  virtual void CancelDriveOperation(int32 operation_id) OVERRIDE;
  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList*) OVERRIDE;
  virtual void ShowNetworkConfigure(const std::string& network_id,
                                    gfx::NativeWindow parent_window) OVERRIDE;
  virtual bool EnrollNetwork(const std::string& network_id,
                             gfx::NativeWindow parent_window) OVERRIDE;
  virtual void ManageBluetoothDevices() OVERRIDE;
  virtual void ToggleBluetooth() OVERRIDE;
  virtual bool IsBluetoothDiscovering() OVERRIDE;
  virtual void ShowMobileSimDialog() OVERRIDE;
  virtual void ShowMobileSetupDialog(const std::string& service_path) OVERRIDE;
  virtual void ShowOtherNetworkDialog(const std::string& type) OVERRIDE;
  virtual bool GetBluetoothAvailable() OVERRIDE;
  virtual bool GetBluetoothEnabled() OVERRIDE;
  virtual bool GetBluetoothDiscovering() OVERRIDE;
  virtual void ChangeProxySettings() OVERRIDE;
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) OVERRIDE;
  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) OVERRIDE;
  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) OVERRIDE;
  virtual int GetSystemTrayMenuWidth() OVERRIDE;
  virtual void ActiveUserWasChanged() OVERRIDE;
  virtual bool IsSearchKeyMappedToCapsLock() OVERRIDE;
  virtual tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) OVERRIDE;

 private:
  bool bluetooth_enabled_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSystemTrayDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_

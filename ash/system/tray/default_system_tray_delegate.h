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

  // Overridden from SystemTrayDelegate:
  void Initialize() override;
  void Shutdown() override;
  bool GetTrayVisibilityOnStartup() override;
  user::LoginStatus GetUserLoginStatus() const override;
  void ChangeProfilePicture() override;
  const std::string GetEnterpriseDomain() const override;
  const base::string16 GetEnterpriseMessage() const override;
  const std::string GetSupervisedUserManager() const override;
  const base::string16 GetSupervisedUserManagerName() const override;
  const base::string16 GetSupervisedUserMessage() const override;
  bool IsUserSupervised() const override;
  bool IsUserChild() const override;
  void GetSystemUpdateInfo(UpdateInfo* info) const override;
  base::HourClockType GetHourClockType() const override;
  void ShowSettings() override;
  bool ShouldShowSettings() override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowNetworkSettings(const std::string& service_path) override;
  void ShowBluetoothSettings() override;
  void ShowDisplaySettings() override;
  void ShowChromeSlow() override;
  bool ShouldShowDisplayNotification() override;
  void ShowIMESettings() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowPublicAccountInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowSupervisedUserInfo() override;
  void ShowUserLogin() override;
  void ShutDown() override;
  void SignOut() override;
  void RequestLockScreen() override;
  void RequestRestartForUpdate() override;
  void GetAvailableBluetoothDevices(BluetoothDeviceList* list) override;
  void BluetoothStartDiscovering() override;
  void BluetoothStopDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  void GetCurrentIME(IMEInfo* info) override;
  void GetAvailableIMEList(IMEInfoList* list) override;
  void GetCurrentIMEProperties(IMEPropertyInfoList* list) override;
  void SwitchIME(const std::string& ime_id) override;
  void ActivateIMEProperty(const std::string& key) override;
  void ManageBluetoothDevices() override;
  void ToggleBluetooth() override;
  bool IsBluetoothDiscovering() override;
  void ShowOtherNetworkDialog(const std::string& type) override;
  bool GetBluetoothAvailable() override;
  bool GetBluetoothEnabled() override;
  bool GetBluetoothDiscovering() override;
  void ChangeProxySettings() override;
  VolumeControlDelegate* GetVolumeControlDelegate() const override;
  void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  int GetSystemTrayMenuWidth() override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;
  tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override;
  void AddCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer) override;
  void RemoveCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer) override;

 private:
  bool bluetooth_enabled_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSystemTrayDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_

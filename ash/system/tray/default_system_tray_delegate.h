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
  virtual void Initialize() override;
  virtual void Shutdown() override;
  virtual bool GetTrayVisibilityOnStartup() override;
  virtual user::LoginStatus GetUserLoginStatus() const override;
  virtual void ChangeProfilePicture() override;
  virtual const std::string GetEnterpriseDomain() const override;
  virtual const base::string16 GetEnterpriseMessage() const override;
  virtual const std::string GetSupervisedUserManager() const override;
  virtual const base::string16 GetSupervisedUserManagerName() const override;
  virtual const base::string16 GetSupervisedUserMessage() const override;
  virtual bool IsUserSupervised() const override;
  virtual void GetSystemUpdateInfo(UpdateInfo* info) const override;
  virtual base::HourClockType GetHourClockType() const override;
  virtual void ShowSettings() override;
  virtual bool ShouldShowSettings() override;
  virtual void ShowDateSettings() override;
  virtual void ShowSetTimeDialog() override;
  virtual void ShowNetworkSettings(const std::string& service_path) override;
  virtual void ShowBluetoothSettings() override;
  virtual void ShowDisplaySettings() override;
  virtual void ShowChromeSlow() override;
  virtual bool ShouldShowDisplayNotification() override;
  virtual void ShowIMESettings() override;
  virtual void ShowHelp() override;
  virtual void ShowAccessibilityHelp() override;
  virtual void ShowAccessibilitySettings() override;
  virtual void ShowPublicAccountInfo() override;
  virtual void ShowEnterpriseInfo() override;
  virtual void ShowSupervisedUserInfo() override;
  virtual void ShowUserLogin() override;
  virtual bool ShowSpringChargerReplacementDialog() override;
  virtual bool IsSpringChargerReplacementDialogVisible() override;
  virtual bool HasUserConfirmedSafeSpringCharger() override;
  virtual void ShutDown() override;
  virtual void SignOut() override;
  virtual void RequestLockScreen() override;
  virtual void RequestRestartForUpdate() override;
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* list) override;
  virtual void BluetoothStartDiscovering() override;
  virtual void BluetoothStopDiscovering() override;
  virtual void ConnectToBluetoothDevice(const std::string& address) override;
  virtual void GetCurrentIME(IMEInfo* info) override;
  virtual void GetAvailableIMEList(IMEInfoList* list) override;
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) override;
  virtual void SwitchIME(const std::string& ime_id) override;
  virtual void ActivateIMEProperty(const std::string& key) override;
  virtual void ShowNetworkConfigure(const std::string& network_id) override;
  virtual bool EnrollNetwork(const std::string& network_id) override;
  virtual void ManageBluetoothDevices() override;
  virtual void ToggleBluetooth() override;
  virtual bool IsBluetoothDiscovering() override;
  virtual void ShowMobileSimDialog() override;
  virtual void ShowMobileSetupDialog(const std::string& service_path) override;
  virtual void ShowOtherNetworkDialog(const std::string& type) override;
  virtual bool GetBluetoothAvailable() override;
  virtual bool GetBluetoothEnabled() override;
  virtual bool GetBluetoothDiscovering() override;
  virtual void ChangeProxySettings() override;
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const override;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) override;
  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) override;
  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) override;
  virtual int GetSystemTrayMenuWidth() override;
  virtual void ActiveUserWasChanged() override;
  virtual bool IsSearchKeyMappedToCapsLock() override;
  virtual tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override;
  virtual void AddCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer) override;
  virtual void RemoveCustodianInfoTrayObserver(
      CustodianInfoTrayObserver* observer) override;

 private:
  bool bluetooth_enabled_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSystemTrayDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DEFAULT_SYSTEM_TRAY_DELEGATE_H_

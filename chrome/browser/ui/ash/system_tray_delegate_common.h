// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_

#include <string>

#include "ash/system/tray/system_tray_delegate.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace ash {
class SystemTrayNotifier;
}

// Common base class for platform-specific implementations.
class SystemTrayDelegateCommon : public ash::SystemTrayDelegate,
                                 public content::NotificationObserver {
 public:
  SystemTrayDelegateCommon();

  virtual ~SystemTrayDelegateCommon();

  // Overridden from ash::SystemTrayDelegate:
  virtual void Initialize() override;
  virtual void Shutdown() override;
  virtual bool GetTrayVisibilityOnStartup() override;
  virtual ash::user::LoginStatus GetUserLoginStatus() const override;
  virtual void ChangeProfilePicture() override;
  virtual const std::string GetEnterpriseDomain() const override;
  virtual const base::string16 GetEnterpriseMessage() const override;
  virtual const std::string GetSupervisedUserManager() const override;
  virtual const base::string16 GetSupervisedUserManagerName() const override;
  virtual const base::string16 GetSupervisedUserMessage() const override;
  virtual bool IsUserSupervised() const override;
  virtual void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
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
  virtual void ShowSupervisedUserInfo() override;
  virtual void ShowEnterpriseInfo() override;
  virtual void ShowUserLogin() override;
  virtual bool ShowSpringChargerReplacementDialog() override;
  virtual bool IsSpringChargerReplacementDialogVisible() override;
  virtual bool HasUserConfirmedSafeSpringCharger() override;
  virtual void ShutDown() override;
  virtual void SignOut() override;
  virtual void RequestLockScreen() override;
  virtual void RequestRestartForUpdate() override;
  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) override;
  virtual void BluetoothStartDiscovering() override;
  virtual void BluetoothStopDiscovering() override;
  virtual void ConnectToBluetoothDevice(const std::string& address) override;
  virtual bool IsBluetoothDiscovering() override;
  virtual void GetCurrentIME(ash::IMEInfo* info) override;
  virtual void GetAvailableIMEList(ash::IMEInfoList* list) override;
  virtual void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  virtual void SwitchIME(const std::string& ime_id) override;
  virtual void ActivateIMEProperty(const std::string& key) override;
  virtual void ShowNetworkConfigure(const std::string& network_id) override;
  virtual bool EnrollNetwork(const std::string& network_id) override;
  virtual void ManageBluetoothDevices() override;
  virtual void ToggleBluetooth() override;
  virtual void ShowMobileSimDialog() override;
  virtual void ShowMobileSetupDialog(const std::string& service_path) override;
  virtual void ShowOtherNetworkDialog(const std::string& type) override;
  virtual bool GetBluetoothAvailable() override;
  virtual bool GetBluetoothEnabled() override;
  virtual bool GetBluetoothDiscovering() override;
  virtual void ChangeProxySettings() override;
  virtual ash::VolumeControlDelegate* GetVolumeControlDelegate() const override;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) override;
  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) override;
  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) override;
  virtual int GetSystemTrayMenuWidth() override;
  virtual void ActiveUserWasChanged() override;
  virtual bool IsSearchKeyMappedToCapsLock() override;
  virtual ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override;
  virtual void AddCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  virtual void RemoveCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void UpdateClockType();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) override;

  scoped_ptr<content::NotificationRegistrar> registrar_;
  base::HourClockType clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateCommon);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_

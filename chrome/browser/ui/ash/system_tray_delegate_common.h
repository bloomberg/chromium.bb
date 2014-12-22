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

  ~SystemTrayDelegateCommon() override;

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  void Shutdown() override;
  bool GetTrayVisibilityOnStartup() override;
  ash::user::LoginStatus GetUserLoginStatus() const override;
  void ChangeProfilePicture() override;
  const std::string GetEnterpriseDomain() const override;
  const base::string16 GetEnterpriseMessage() const override;
  const std::string GetSupervisedUserManager() const override;
  const base::string16 GetSupervisedUserManagerName() const override;
  const base::string16 GetSupervisedUserMessage() const override;
  bool IsUserSupervised() const override;
  bool IsUserChild() const override;
  void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
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
  void ShowSupervisedUserInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowUserLogin() override;
  void ShutDown() override;
  void SignOut() override;
  void RequestLockScreen() override;
  void RequestRestartForUpdate() override;
  void GetAvailableBluetoothDevices(ash::BluetoothDeviceList* list) override;
  void BluetoothStartDiscovering() override;
  void BluetoothStopDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  bool IsBluetoothDiscovering() override;
  void GetCurrentIME(ash::IMEInfo* info) override;
  void GetAvailableIMEList(ash::IMEInfoList* list) override;
  void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  void SwitchIME(const std::string& ime_id) override;
  void ActivateIMEProperty(const std::string& key) override;
  void ManageBluetoothDevices() override;
  void ToggleBluetooth() override;
  void ShowOtherNetworkDialog(const std::string& type) override;
  bool GetBluetoothAvailable() override;
  bool GetBluetoothEnabled() override;
  bool GetBluetoothDiscovering() override;
  void ChangeProxySettings() override;
  ash::VolumeControlDelegate* GetVolumeControlDelegate() const override;
  void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  int GetSystemTrayMenuWidth() override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;
  ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override;
  void AddCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  void RemoveCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void UpdateClockType();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  scoped_ptr<content::NotificationRegistrar> registrar_;
  base::HourClockType clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateCommon);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_

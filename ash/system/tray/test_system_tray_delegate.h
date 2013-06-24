// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/system/tray/system_tray_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

// TODO(oshima/stevenjb): Move this to ash/test. crbug.com/159693.

namespace ash {
namespace test {

class TestSystemTrayDelegate : public SystemTrayDelegate {
 public:
  TestSystemTrayDelegate();

  virtual ~TestSystemTrayDelegate();

 public:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE;

  // Overridden from SystemTrayDelegate:
  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE;
  virtual bool IsOobeCompleted() const OVERRIDE;
  virtual void ChangeProfilePicture() OVERRIDE;
  virtual const std::string GetEnterpriseDomain() const OVERRIDE;
  virtual const base::string16 GetEnterpriseMessage() const OVERRIDE;
  virtual const std::string GetLocallyManagedUserManager() const OVERRIDE;
  virtual const base::string16 GetLocallyManagedUserManagerName() const
      OVERRIDE;
  virtual const base::string16 GetLocallyManagedUserMessage() const OVERRIDE;
  virtual bool SystemShouldUpgrade() const OVERRIDE;
  virtual base::HourClockType GetHourClockType() const OVERRIDE;
  virtual void ShowSettings() OVERRIDE;
  virtual void ShowDateSettings() OVERRIDE;
  virtual void ShowNetworkSettings(const std::string& service_path) OVERRIDE;
  virtual void ShowBluetoothSettings() OVERRIDE;
  virtual void ShowDisplaySettings() OVERRIDE;
  virtual bool ShouldShowDisplayNotification() OVERRIDE;
  virtual void ShowDriveSettings() OVERRIDE;
  virtual void ShowIMESettings() OVERRIDE;
  virtual void ShowHelp() OVERRIDE;
  virtual void ShowAccessibilityHelp() OVERRIDE;
  virtual void ShowAccessibilitySettings() OVERRIDE;
  virtual void ShowPublicAccountInfo() OVERRIDE;
  virtual void ShowEnterpriseInfo() OVERRIDE;
  virtual void ShowLocallyManagedUserInfo() OVERRIDE;
  virtual void ShowUserLogin() OVERRIDE;
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
  virtual void ConfigureNetwork(const std::string& network_id) OVERRIDE;
  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE;
  virtual void AddBluetoothDevice() OVERRIDE;
  virtual void ToggleBluetooth() OVERRIDE;
  virtual bool IsBluetoothDiscovering() OVERRIDE;
  virtual void ShowMobileSimDialog() OVERRIDE;
  virtual void ShowOtherWifi() OVERRIDE;
  virtual void ShowOtherVPN() OVERRIDE;
  virtual void ShowOtherCellular() OVERRIDE;
  virtual bool GetBluetoothAvailable() OVERRIDE;
  virtual bool GetBluetoothEnabled() OVERRIDE;
  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) OVERRIDE;
  virtual void ShowCellularURL(const std::string& url) OVERRIDE;
  virtual void ChangeProxySettings() OVERRIDE;
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) OVERRIDE;
  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) OVERRIDE;
  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) OVERRIDE;
  virtual int GetSystemTrayMenuWidth() OVERRIDE;
  virtual base::string16 FormatTimeDuration(
      const base::TimeDelta& delta) const OVERRIDE;
  virtual void MaybeSpeak(const std::string& utterance) const OVERRIDE;

  void set_should_show_display_notification(bool should_show) {
    should_show_display_notification_ = should_show;
  }

 private:
  bool bluetooth_enabled_;
  bool caps_lock_enabled_;
  bool should_show_display_notification_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

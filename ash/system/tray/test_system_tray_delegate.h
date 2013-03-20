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
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE;

  // Overridden from SystemTrayDelegate:
  virtual const string16 GetUserDisplayName() const OVERRIDE;
  virtual const std::string GetUserEmail() const OVERRIDE;
  virtual const gfx::ImageSkia& GetUserImage() const OVERRIDE;
  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE;
  virtual void ChangeProfilePicture() OVERRIDE;
  virtual const std::string GetEnterpriseDomain() const OVERRIDE;
  virtual const string16 GetEnterpriseMessage() const OVERRIDE;
  virtual bool SystemShouldUpgrade() const OVERRIDE;
  virtual base::HourClockType GetHourClockType() const OVERRIDE;
  virtual PowerSupplyStatus GetPowerSupplyStatus() const OVERRIDE;
  virtual void RequestStatusUpdate() const OVERRIDE;
  virtual void ShowSettings() OVERRIDE;
  virtual void ShowDateSettings() OVERRIDE;
  virtual void ShowNetworkSettings() OVERRIDE;
  virtual void ShowBluetoothSettings() OVERRIDE;
  virtual void ShowDisplaySettings() OVERRIDE;
  virtual void ShowDriveSettings() OVERRIDE;
  virtual void ShowIMESettings() OVERRIDE;
  virtual void ShowHelp() OVERRIDE;
  virtual void ShowAccessibilityHelp() OVERRIDE;
  virtual void ShowPublicAccountInfo() OVERRIDE;
  virtual void ShowEnterpriseInfo() OVERRIDE;
  virtual void ShutDown() OVERRIDE;
  virtual void SignOut() OVERRIDE;
  virtual void RequestLockScreen() OVERRIDE;
  virtual void RequestRestart() OVERRIDE;
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* list) OVERRIDE;
  virtual void BluetoothStartDiscovering() OVERRIDE;
  virtual void BluetoothStopDiscovering() OVERRIDE;
  virtual void ToggleBluetoothConnection(const std::string& address) OVERRIDE;
  virtual void GetCurrentIME(IMEInfo* info) OVERRIDE;
  virtual void GetAvailableIMEList(IMEInfoList* list) OVERRIDE;
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) OVERRIDE;
  virtual void SwitchIME(const std::string& ime_id) OVERRIDE;
  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE;
  virtual void CancelDriveOperation(const base::FilePath&) OVERRIDE;
  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList*) OVERRIDE;
  virtual void GetMostRelevantNetworkIcon(NetworkIconInfo* info,
                                          bool large) OVERRIDE;
  virtual void GetVirtualNetworkIcon(ash::NetworkIconInfo* info) OVERRIDE;
  virtual void GetAvailableNetworks(
      std::vector<NetworkIconInfo>* list) OVERRIDE;
  virtual void GetVirtualNetworks(std::vector<NetworkIconInfo>* list) OVERRIDE;
  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE;
  virtual void GetNetworkAddresses(std::string* ip_address,
                                   std::string* ethernet_mac_address,
                                   std::string* wifi_mac_address) OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;
  virtual void AddBluetoothDevice() OVERRIDE;
  virtual void ToggleAirplaneMode() OVERRIDE;
  virtual void ToggleWifi() OVERRIDE;
  virtual void ToggleMobile() OVERRIDE;
  virtual void ToggleBluetooth() OVERRIDE;
  virtual bool IsBluetoothDiscovering() OVERRIDE;
  virtual void ShowOtherWifi() OVERRIDE;
  virtual void ShowOtherVPN() OVERRIDE;
  virtual void ShowOtherCellular() OVERRIDE;
  virtual bool IsNetworkConnected() OVERRIDE;
  virtual bool GetWifiAvailable() OVERRIDE;
  virtual bool GetMobileAvailable() OVERRIDE;
  virtual bool GetBluetoothAvailable() OVERRIDE;
  virtual bool GetWifiEnabled() OVERRIDE;
  virtual bool GetMobileEnabled() OVERRIDE;
  virtual bool GetBluetoothEnabled() OVERRIDE;
  virtual bool GetMobileScanSupported() OVERRIDE;
  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) OVERRIDE;
  virtual bool GetWifiScanning() OVERRIDE;
  virtual bool GetCellularInitializing() OVERRIDE;
  virtual void ShowCellularURL(const std::string& url) OVERRIDE;
  virtual void ChangeProxySettings() OVERRIDE;
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) OVERRIDE;
  virtual base::Time GetSessionStartTime() OVERRIDE;
  virtual base::TimeDelta GetSessionLengthLimit() OVERRIDE;
  virtual int GetSystemTrayMenuWidth() OVERRIDE;

 private:
  bool wifi_enabled_;
  bool cellular_enabled_;
  bool bluetooth_enabled_;
  bool caps_lock_enabled_;
  gfx::ImageSkia null_image_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/user/login_status.h"
#include "base/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct ASH_EXPORT NetworkIconInfo {
  NetworkIconInfo();
  ~NetworkIconInfo();

  bool highlight;
  bool tray_icon_visible;
  gfx::ImageSkia image;
  string16 name;
  string16 description;
  std::string service_path;
};

struct ASH_EXPORT BluetoothDeviceInfo {
  BluetoothDeviceInfo();
  ~BluetoothDeviceInfo();

  std::string address;
  string16 display_name;
  bool connected;
};

typedef std::vector<BluetoothDeviceInfo> BluetoothDeviceList;

// Structure that packs progress information of each operation.
struct ASH_EXPORT DriveOperationStatus {
  enum OperationType {
    OPERATION_UPLOAD,
    OPERATION_DOWNLOAD,
    OPERATION_OTHER,
  };

  enum OperationState {
    OPERATION_NOT_STARTED,
    OPERATION_STARTED,
    OPERATION_IN_PROGRESS,
    OPERATION_COMPLETED,
    OPERATION_FAILED,
    OPERATION_SUSPENDED,
  };

  DriveOperationStatus();
  ~DriveOperationStatus();

  // File path.
  FilePath file_path;
  // Current operation completion progress [0.0 - 1.0].
  double progress;
  OperationType type;
  OperationState state;
};

typedef std::vector<DriveOperationStatus> DriveOperationStatusList;


struct ASH_EXPORT IMEPropertyInfo {
  IMEPropertyInfo();
  ~IMEPropertyInfo();

  bool selected;
  std::string key;
  string16 name;
};

typedef std::vector<IMEPropertyInfo> IMEPropertyInfoList;

struct ASH_EXPORT IMEInfo {
  IMEInfo();
  ~IMEInfo();

  bool selected;
  bool third_party;
  std::string id;
  string16 name;
  string16 short_name;
};

typedef std::vector<IMEInfo> IMEInfoList;

class VolumeControlDelegate;

class SystemTrayDelegate {
 public:
  virtual ~SystemTrayDelegate() {}

  // Returns true if system tray should be visible on startup.
  virtual bool GetTrayVisibilityOnStartup() = 0;

  // Gets information about the logged in user.
  virtual const string16 GetUserDisplayName() const = 0;
  virtual const std::string GetUserEmail() const = 0;
  virtual const gfx::ImageSkia& GetUserImage() const = 0;
  virtual user::LoginStatus GetUserLoginStatus() const = 0;

  // Returns whether a system upgrade is available.
  virtual bool SystemShouldUpgrade() const = 0;

  // Returns the desired hour clock type.
  virtual base::HourClockType GetHourClockType() const = 0;

  // Gets the current power supply status.
  virtual PowerSupplyStatus GetPowerSupplyStatus() const = 0;

  // Requests a status update.
  virtual void RequestStatusUpdate() const = 0;

  // Shows settings.
  virtual void ShowSettings() = 0;

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings() = 0;

  // Shows the settings related to network.
  virtual void ShowNetworkSettings() = 0;

  // Shows the settings related to bluetooth.
  virtual void ShowBluetoothSettings() = 0;

  // Shows settings related to multiple displays.
  virtual void ShowDisplaySettings() = 0;

  // Shows settings related to Google Drive.
  virtual void ShowDriveSettings() = 0;

  // Shows settings related to input methods.
  virtual void ShowIMESettings() = 0;

  // Shows help.
  virtual void ShowHelp() = 0;

  // Attempts to shut down the system.
  virtual void ShutDown() = 0;

  // Attempts to sign out the user.
  virtual void SignOut() = 0;

  // Attempts to lock the screen.
  virtual void RequestLockScreen() = 0;

  // Attempts to restart the system.
  virtual void RequestRestart() = 0;

  // Returns a list of available bluetooth devices.
  virtual void GetAvailableBluetoothDevices(BluetoothDeviceList* devices) = 0;

  // Toggles connection to a specific bluetooth device.
  virtual void ToggleBluetoothConnection(const std::string& address) = 0;

  // Returns the currently selected IME.
  virtual void GetCurrentIME(IMEInfo* info) = 0;

  // Returns a list of availble IMEs.
  virtual void GetAvailableIMEList(IMEInfoList* list) = 0;

  // Returns a list of properties for the currently selected IME.
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) = 0;

  // Switches to the selected input method.
  virtual void SwitchIME(const std::string& ime_id) = 0;

  // Activates an IME property.
  virtual void ActivateIMEProperty(const std::string& key) = 0;

  // Cancels ongoing drive operation.
  virtual void CancelDriveOperation(const FilePath& file_path) = 0;

  // Returns information about the ongoing drive operations.
  virtual void GetDriveOperationStatusList(
      DriveOperationStatusList* list) = 0;

  // Returns information about the most relevant network. Relevance is
  // determined by the implementor (e.g. a connecting network may be more
  // relevant over a connected network etc.)
  virtual void GetMostRelevantNetworkIcon(NetworkIconInfo* info,
                                          bool large) = 0;

  // Returns information about the available networks.
  virtual void GetAvailableNetworks(std::vector<NetworkIconInfo>* list) = 0;

  // Connects to the network specified by the unique id.
  virtual void ConnectToNetwork(const std::string& network_id) = 0;

  // Gets the network IP address, and the mac addresses for the ethernet and
  // wifi devices. If any of this is unavailable, empty strings are returned.
  virtual void GetNetworkAddresses(std::string* ip_address,
                                   std::string* ethernet_mac_address,
                                   std::string* wifi_mac_address) = 0;

  // Requests network scan when list of networks is opened.
  virtual void RequestNetworkScan() = 0;

  // Shous UI to add a new bluetooth device.
  virtual void AddBluetoothDevice() = 0;

  // Toggles airplane mode.
  virtual void ToggleAirplaneMode() = 0;

  // Toggles wifi network.
  virtual void ToggleWifi() = 0;

  // Toggles mobile network.
  virtual void ToggleMobile() = 0;

  // Toggles bluetooth.
  virtual void ToggleBluetooth() = 0;

  // Shows UI to connect to an unlisted wifi network.
  virtual void ShowOtherWifi() = 0;

  // Shows UI to search for cellular networks.
  virtual void ShowOtherCellular() = 0;

  // Returns whether the system is connected to any network.
  virtual bool IsNetworkConnected() = 0;

  // Returns whether wifi is available.
  virtual bool GetWifiAvailable() = 0;

  // Returns whether mobile networking (cellular or wimax) is available.
  virtual bool GetMobileAvailable() = 0;

  // Returns whether bluetooth capability is available.
  virtual bool GetBluetoothAvailable() = 0;

  // Returns whether wifi is enabled.
  virtual bool GetWifiEnabled() = 0;

  // Returns whether mobile (cellular or wimax) networking is enabled.
  virtual bool GetMobileEnabled() = 0;

  // Returns whether bluetooth is enabled.
  virtual bool GetBluetoothEnabled() = 0;

  // Returns whether mobile scanning is supported.
  virtual bool GetMobileScanSupported() = 0;

  // Retrieves information about the carrier and locale specific |setup_url|.
  // If none of the carrier info/setup URL cannot be retrieved, returns false.
  // Note: |setup_url| is returned when carrier is not defined (no SIM card).
  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) = 0;

  // Opens the cellular network specific URL.
  virtual void ShowCellularURL(const std::string& url) = 0;

  // Shows UI for changing proxy settings.
  virtual void ChangeProxySettings() = 0;

  // Returns VolumeControlDelegate.
  virtual VolumeControlDelegate* GetVolumeControlDelegate() const = 0;

  // Sets VolumeControlDelegate.
  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) = 0;

};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

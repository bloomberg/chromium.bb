// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_manager.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_options_page_ui_handler.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler : public chromeos::CrosOptionsPageUIHandler,
                                public chromeos::BluetoothManager::Observer,
                                public chromeos::BluetoothAdapter::Observer {
 public:
  BluetoothOptionsHandler();
  virtual ~BluetoothOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings);
  virtual void Initialize();
  virtual void RegisterMessages();

  // Called when the 'Enable bluetooth' checkbox value is changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void EnableChangeCallback(const base::ListValue* args);

  // Called when the 'Find Devices' button is pressed from the Bluetooth
  // ssettings.
  // |args| will be an empty list.
  void FindDevicesCallback(const base::ListValue* args);

  // Called when the user requests to connect to or disconnect from a Bluetooth
  // device.
  // |args| will be a list containing two arguments, the first argument is the
  // device ID and the second is the requested action.
  void UpdateDeviceCallback(const base::ListValue* args);

  // Sends a notification to the Web UI of the status of a bluetooth device.
  // |device| is the decription of the device.  The supported dictionary keys
  // for device are "deviceName", "deviceId", "deviceType" and "deviceStatus".
  void DeviceNotification(const base::DictionaryValue& device);

  // chromeos::BluetoothManager::Observer override.
  virtual void DefaultAdapterChanged(chromeos::BluetoothAdapter* adapter);

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DiscoveryStarted(const std::string& adapter_id);

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DiscoveryEnded(const std::string& adapter_id);

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DeviceFound(const std::string& adapter_id,
                           chromeos::BluetoothDevice* device);

 private:
  // Compares |adapter| with our cached default adapter ID and calls
  // DefaultAdapterChanged if there has been an unexpected change.
  void ValidateDefaultAdapter(chromeos::BluetoothAdapter* adapter);

  // Simulates extracting a list of available bluetooth devices.
  // Called when emulating ChromeOS from a desktop environment.
  void GenerateFakeDeviceList();

  // The id of the current default bluetooth adapter.
  // The empty string represents "none".
  std::string default_adapter_id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

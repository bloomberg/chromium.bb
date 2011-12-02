// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_manager.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler : public OptionsPageUIHandler,
                                public chromeos::BluetoothManager::Observer,
                                public chromeos::BluetoothAdapter::Observer {
 public:
  BluetoothOptionsHandler();
  virtual ~BluetoothOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

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
  // |args| will be a list containing two or three arguments, the first argument
  // is the device ID and the second is the requested action.  If a third
  // argument is present, it is the passkey for pairing confirmation.
  void UpdateDeviceCallback(const base::ListValue* args);

  // Sends a notification to the Web UI of the status of a Bluetooth device.
  // |device| is the Bluetooth device.
  // |params| is an optional set of parameters.
  void SendDeviceNotification(chromeos::BluetoothDevice* device,
                              base::DictionaryValue* params);

  // Displays a passkey for a device, requesting user confirmation that the
  // key matches an expected value (value displayed on a smartphone for
  // example).
  // |device| is the Bluetooth device being paired.
  // |passkey| is the passkey to display for confirmation.
  void RequestConfirmation(chromeos::BluetoothDevice* device,
                           int passkey);

  // Displays a passkey for a device, which is being typed remotely. During
  // the pairing process, this method may be called repeatedly to track the
  // number of characters entered.  This method is commonly used for pairing
  // keyboards.
  // |device| is the Bluetooth device being paired.
  // |passkey| is the required passkey.
  // |entered| is the number of characters that have already been entered on
  // the remote device.
  void DisplayPasskey(chromeos::BluetoothDevice* device,
                      int passkey,
                      int entered);

  // Displays a blank field for entering a passkey.  The passkey may be
  // a set value specified by the manufacturer of the Bluetooth device, or
  // on a remote display.  The validation is asychronous, and a call is made
  // to |ValidatePasskeyCallback| when the passkey entry is complete.
  // |device| is the Bluetooth device being paired.
  void RequestPasskey(chromeos::BluetoothDevice* device);

  // Callback to validate a user entered passkey.
  // |args| is a list containing the device address and entered passkey.
  void ValidatePasskeyCallback(const base::ListValue* args);

  // chromeos::BluetoothManager::Observer override.
  virtual void DefaultAdapterChanged(
      chromeos::BluetoothAdapter* adapter) OVERRIDE;

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DiscoveryStarted(const std::string& adapter_id) OVERRIDE;

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DiscoveryEnded(const std::string& adapter_id) OVERRIDE;

  // chromeos::BluetoothAdapter::Observer override.
  virtual void DeviceFound(const std::string& adapter_id,
                           chromeos::BluetoothDevice* device) OVERRIDE;

 private:
  // Compares |adapter| with our cached default adapter ID and calls
  // DefaultAdapterChanged if there has been an unexpected change.
  void ValidateDefaultAdapter(chromeos::BluetoothAdapter* adapter);

  // Simulates extracting a list of available bluetooth devices.
  // Called when emulating ChromeOS from a desktop environment.
  void GenerateFakeDeviceList();

  // Simulates the discovery or pairing of a Bluetooth device.  Used when
  // emulating ChromeOS from a desktop environment.
  // |name| is the display name for the device.
  // |address| is the unique Mac address of the device.
  // |icon| is the base name of the icon to use for the device and corresponds
  // to the general device category (e.g. mouse or keyboard).
  // |paired| indicates if the device is paired.
  // |connected| indicates if the device is connected.
  // |pairing| indicates the type of pairing operation.
  void GenerateFakeDevice(const std::string& name,
                          const std::string& address,
                          const std::string& icon,
                          bool paired,
                          bool connected,
                          const std::string& pairing);

  // The id of the current default bluetooth adapter.
  // The empty string represents "none".
  std::string default_adapter_id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

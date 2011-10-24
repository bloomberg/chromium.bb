// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/options/chromeos/cros_options_page_ui_handler.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler : public chromeos::CrosOptionsPageUIHandler {
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

 private:
  // Simulates extracting a list of available bluetooth devices.
  // Called when emulating ChromeOS from a desktop environment.
  void GenerateFakeDeviceList();

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

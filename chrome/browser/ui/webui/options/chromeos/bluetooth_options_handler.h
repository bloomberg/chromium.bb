// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler : public OptionsPageUIHandler,
                                public chromeos::BluetoothAdapter::Observer {
 public:
  BluetoothOptionsHandler();
  virtual ~BluetoothOptionsHandler();

  // Potential errors during the process of pairing or connecting to a
  // Bluetooth device.  Each enumerated value is associated with an i18n
  // label for display in the Bluetooth UI.
  enum ConnectionError {
    DEVICE_NOT_FOUND,
    INCORRECT_PIN,
    CONNECTION_TIMEOUT,
    CONNECTION_REJECTED
  };

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
  void SendDeviceNotification(const BluetoothDevice* device,
                              base::DictionaryValue* params);

  // Displays a passkey for a device, requesting user confirmation that the
  // key matches an expected value (value displayed on a smartphone for
  // example).
  // |device| is the Bluetooth device being paired.
  // |passkey| is the passkey to display for confirmation.
  void RequestConfirmation(const BluetoothDevice* device,
                           int passkey);

  // Displays a passkey for a device, which is being typed remotely. During
  // the pairing process, this method may be called repeatedly to track the
  // number of characters entered.  This method is commonly used for pairing
  // keyboards.
  // |device| is the Bluetooth device being paired.
  // |passkey| is the required passkey.
  // |entered| is the number of characters that have already been entered on
  // the remote device.
  void DisplayPasskey(const BluetoothDevice* device,
                      int passkey,
                      int entered);

  // Displays a blank field for entering a passkey.  The passkey may be
  // a set value specified by the manufacturer of the Bluetooth device, or
  // on a remote display.  The validation is asychronous, and a call is made
  // to |ValidatePasskeyCallback| when the passkey entry is complete.
  // |device| is the Bluetooth device being paired.
  void RequestPasskey(const BluetoothDevice* device);

  // Displays an error that occurred during the pairing or connection process.
  // |device| is the Bluetooth device being paired or connected.
  // |error| is the type of error that occurred.
  void ReportError(const BluetoothDevice* device, ConnectionError error);

  // BluetoothAdapter::Observer implementation.
  virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE;
  virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE;
  virtual void DeviceAdded(BluetoothAdapter* adapter,
                           BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE;

 private:
  // Called by BluetoothAdapter in response to our method calls in case of
  // error.
  void ErrorCallback();

  // Default bluetooth adapter, used for all operations. Owned by this object.
  scoped_ptr<BluetoothAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

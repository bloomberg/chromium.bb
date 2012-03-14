// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER2_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace options2 {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler : public OptionsPageUIHandler,
                                public chromeos::BluetoothAdapter::Observer,
                                public BluetoothDevice::PairingDelegate {
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
  virtual void RegisterMessages() OVERRIDE;

  // Sends a notification to the Web UI of the status of a Bluetooth device.
  // |device| is the Bluetooth device.
  // |params| is an optional set of parameters.
  void SendDeviceNotification(const BluetoothDevice* device,
                              base::DictionaryValue* params);

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // PIN Code for authentication of the device |device|, the UI will display
  // a blank entry form to obtain the PIN code from the user.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void RequestPinCode(BluetoothDevice* device) OVERRIDE;

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // Passkey for authentication of the device |device|, the UI will display
  // a blank entry form to obtain the passkey from the user (a numeric in the
  // range 0-999999).
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing.
  virtual void RequestPasskey(BluetoothDevice* device) OVERRIDE;

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user enter the PIN code |pincode| into the device |device| so that it
  // may be authenticated, the UI will display the PIN code with accompanying
  // instructions.
  //
  // This is used for Bluetooth 2.0 and earlier keyboard devices, the
  // |pincode| will always be a six-digit numeric in the range 000000-999999
  // for compatibilty with later specifications.
  virtual void DisplayPinCode(BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE;

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user enter the Passkey |passkey| into the device |device| so that it
  // may be authenticated, the UI will display the passkey with accompanying
  // instructions.
  //
  // This is used for Bluetooth 2.1 and later devices that support input
  // but not display, such as keyboards. The Passkey is a numeric in the
  // range 0-999999 and should be always presented zero-padded to six
  // digits.
  virtual void DisplayPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user confirm that the Passkey |passkey| is displayed on the screen
  // of the device |device| so that it may be authenticated, the UI will
  // display the passkey with accompanying instructions.
  //
  // This is used for Bluetooth 2.1 and later devices that support display,
  // such as other computers or phones. The Passkey is a numeric in the
  // range 0-999999 and should be always present zero-padded to six
  // digits.
  virtual void ConfirmPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;

  // BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when any previous DisplayPinCode(),
  // DisplayPasskey() or ConfirmPasskey() request should be concluded
  // and removed from the user.
  virtual void DismissDisplayOrConfirm() OVERRIDE;

  // Displays an error that occurred during the pairing or connection process.
  // |device| is the Bluetooth device being paired or connected.
  // |error| is the type of error that occurred.
  void ReportError(const BluetoothDevice* device, ConnectionError error);

  // BluetoothAdapter::Observer implementation.
  virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE;
  virtual void DeviceAdded(BluetoothAdapter* adapter,
                           BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE;

 private:
  // Called by BluetoothAdapter in response to our method calls in case of
  // error.
  void ErrorCallback();

  // Called on completion of initialization of the settings page to update
  // the Bluetooth controls.
  // |args| will be and empty list.
  void InitializeBluetoothStatusCallback(const base::ListValue* args);

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

  // Called when the "Add a device" dialog closes to stop the discovery
  // process.
  // |args| will be an empty list.
  void StopDiscoveryCallback(const base::ListValue* args);

  // Called when the list of paired devices is initialized in order to
  // populate the list.
  // |args| will be an empty list.
  void GetPairedDevicesCallback(const base::ListValue* args);

  // Default bluetooth adapter, used for all operations. Owned by this object.
  scoped_ptr<BluetoothAdapter> adapter_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than this object does.
  base::WeakPtrFactory<BluetoothOptionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER2_H_

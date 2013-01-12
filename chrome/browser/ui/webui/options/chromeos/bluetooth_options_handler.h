// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace options {

// Handler for Bluetooth options on the system options page.
class BluetoothOptionsHandler
    : public ::options::OptionsPageUIHandler,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate {
 public:
  BluetoothOptionsHandler();
  virtual ~BluetoothOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  void InitializeAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Sends a notification to the Web UI of the status of a Bluetooth device.
  // |device| is the Bluetooth device.
  // |params| is an optional set of parameters.
  void SendDeviceNotification(const device::BluetoothDevice* device,
                              base::DictionaryValue* params);

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // PIN Code for authentication of the device |device|, the UI will display
  // a blank entry form to obtain the PIN code from the user.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void RequestPinCode(device::BluetoothDevice* device) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // Passkey for authentication of the device |device|, the UI will display
  // a blank entry form to obtain the passkey from the user (a numeric in the
  // range 0-999999).
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing.
  virtual void RequestPasskey(device::BluetoothDevice* device) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user enter the PIN code |pincode| into the device |device| so that it
  // may be authenticated, the UI will display the PIN code with accompanying
  // instructions.
  //
  // This is used for Bluetooth 2.0 and earlier keyboard devices, the
  // |pincode| will always be a six-digit numeric in the range 000000-999999
  // for compatibilty with later specifications.
  virtual void DisplayPinCode(device::BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate override.
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
  virtual void DisplayPasskey(
      device::BluetoothDevice* device, uint32 passkey) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate override.
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
  virtual void ConfirmPasskey(
      device::BluetoothDevice* device, uint32 passkey) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when any previous DisplayPinCode(),
  // DisplayPasskey() or ConfirmPasskey() request should be concluded
  // and removed from the user.
  virtual void DismissDisplayOrConfirm() OVERRIDE;

  // Displays a Bluetooth error.
  // |error| maps to a localized resource for the error message.
  // |address| is the address of the Bluetooth device.  May be an empty
  // string if the error is not specific to a single device.
  void ReportError(const std::string& error, const std::string& address);

  // device::BluetoothAdapter::Observer implementation.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

 private:
  // Called by device::BluetoothAdapter in response to a failure to
  // change the power status of the adapter.
  void EnableChangeError();

  // Called by device::BluetoothAdapter in response to a failure to
  // set the adapter into discovery mode.
  void FindDevicesError();

  // Called by device::BluetoothAdapter in response to a failure to
  // remove the adapter from discovery mode.
  void StopDiscoveryError();

  // Called by device::BluetoothDevice in response to a failure to
  // connect to the device with bluetooth address |address| due to an error
  // encoded in |error_code|.
  void ConnectError(const std::string& address,
                    device::BluetoothDevice::ConnectErrorCode error_code);

  // Called by device::BluetoothDevice in response to a failure to
  // disconnect the device with bluetooth address |address|.
  void DisconnectError(const std::string& address);

  // Called by device::BluetoothDevice in response to a failure to
  // disconnect and unpair the device with bluetooth address |address|.
  void ForgetError(const std::string& address);

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

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than this object does.
  base::WeakPtrFactory<BluetoothOptionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

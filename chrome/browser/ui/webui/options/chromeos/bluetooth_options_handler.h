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
  ~BluetoothOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void RegisterMessages() override;
  void InitializeHandler() override;
  void InitializePage() override;

  void InitializeAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Sends a notification to the Web UI of the status of a Bluetooth device.
  // |device| is the Bluetooth device.
  // |params| is an optional set of parameters.
  // |pairing| is an optional pairing command.
  void SendDeviceNotification(const device::BluetoothDevice* device,
                              base::DictionaryValue* params,
                              std::string pairing);

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // PIN Code for authentication of the device |device|, the UI will display
  // a blank entry form to obtain the PIN code from the user.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  void RequestPinCode(device::BluetoothDevice* device) override;

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
  void RequestPasskey(device::BluetoothDevice* device) override;

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
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;

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
  void DisplayPasskey(device::BluetoothDevice* device, uint32 passkey) override;

  // device::BluetoothDevice::PairingDelegate override.
  //
  // This method will be called when the Bluetooth daemon gets a notification
  // of a key entered on the device |device| while pairing with the device
  // using a PIN code or a Passkey.
  //
  // The UI will show a visual indication that a given key was pressed in the
  // same pairing overlay where the PIN code or Passkey is displayed.
  //
  // A first call with |entered| as 0 will indicate that this notification
  // mechanism is supported by the device allowing the UI to display this fact.
  // A last call with |entered| as the length of the key plus one will indicate
  // that the [enter] key was pressed.
  void KeysEntered(device::BluetoothDevice* device, uint32 entered) override;

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
  void ConfirmPasskey(device::BluetoothDevice* device, uint32 passkey) override;

  // device::BluetoothDevice::PairingDelegate override.
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // Displays a Bluetooth error.
  // |error| maps to a localized resource for the error message.
  // |address| is the address of the Bluetooth device.  May be an empty
  // string if the error is not specific to a single device.
  void ReportError(const std::string& error, const std::string& address);

  // device::BluetoothAdapter::Observer implementation.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  // Displays in the UI a connecting to the device |device| message.
  void DeviceConnecting(device::BluetoothDevice* device);

  // Called by device::BluetoothAdapter in response to a failure to
  // change the power status of the adapter.
  void EnableChangeError();

  // Called by device::BluetoothDevice on a successful pairing and connection
  // to a device.
  void Connected();

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

  // Called when the user requests to connect to or disconnect from a Bluetooth
  // device.
  // |args| will be a list containing two or three arguments, the first argument
  // is the device ID and the second is the requested action.  If a third
  // argument is present, it is the passkey for pairing confirmation.
  void UpdateDeviceCallback(const base::ListValue* args);

  // Called when the list of paired devices is initialized in order to
  // populate the list.
  // |args| will be an empty list.
  void GetPairedDevicesCallback(const base::ListValue* args);

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Cached information about the current pairing device, if any.
  std::string pairing_device_address_;
  std::string pairing_device_pairing_;
  std::string pairing_device_pincode_;
  int pairing_device_passkey_;
  int pairing_device_entered_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than this object does.
  base::WeakPtrFactory<BluetoothOptionsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_BLUETOOTH_OPTIONS_HANDLER_H_

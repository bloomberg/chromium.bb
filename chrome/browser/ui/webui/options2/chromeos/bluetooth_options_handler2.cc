// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/bluetooth_options_handler2.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/ui/webui/options2/chromeos/system_settings_provider2.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// |UpdateDeviceCallback| takes a variable length list as an argument. The
// value stored in each list element is indicated by the following constants.
const int kUpdateDeviceAddressIndex = 0;
const int kUpdateDeviceCommandIndex = 1;
const int kUpdateDevicePasskeyIndex = 2;

}  // namespace

namespace chromeos {
namespace options2 {

BluetoothOptionsHandler::BluetoothOptionsHandler() {
}

BluetoothOptionsHandler::~BluetoothOptionsHandler() {
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kEnableBluetooth)) {
    return;
  }

  adapter_->RemoveObserver(this);
}

void BluetoothOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("bluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH));
  localized_strings->SetString("disableBluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_DISABLE));
  localized_strings->SetString("enableBluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_ENABLE));
  localized_strings->SetString("addBluetoothDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_ADD_BLUETOOTH_DEVICE));
  localized_strings->SetString("bluetoothAddDeviceTitle",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ADD_DEVICE_TITLE));
  localized_strings->SetString("bluetoothOptionsPageTabTitle",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ADD_DEVICE_TITLE));
  localized_strings->SetString("findBluetoothDevices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_FIND_BLUETOOTH_DEVICES));
  localized_strings->SetString("bluetoothNoDevices",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES));
  localized_strings->SetString("bluetoothNoDevicesFound",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND));
  localized_strings->SetString("bluetoothScanning",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING));
  localized_strings->SetString("bluetoothDeviceConnected",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTED));
  localized_strings->SetString("bluetoothDeviceNotConnected",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_NOT_CONNECTED));
  localized_strings->SetString("bluetoothConnectDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT));
  localized_strings->SetString("bluetoothDisconnectDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT));
  localized_strings->SetString("bluetoothForgetDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_FORGET));
  localized_strings->SetString("bluetoothCancel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CANCEL));
  localized_strings->SetString("bluetoothEnterKey",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_KEY));
  localized_strings->SetString("bluetoothAcceptPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY));
  localized_strings->SetString("bluetoothRejectPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_REJECT_PASSKEY));
  localized_strings->SetString("bluetoothEnterPinCode",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_PIN_CODE_REQUEST));
  localized_strings->SetString("bluetoothEnterPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothRemotePinCode",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_REMOTE_PIN_CODE_REQUEST));
  localized_strings->SetString("bluetoothRemotePasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_REMOTE_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothConfirmPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothDismissError",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_DISMISS_ERROR));
  localized_strings->SetString("bluetoothErrorNoDevice",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTION_FAILED_NO_DEVICE));
  localized_strings->SetString("bluetoothErrorIncorrectPin",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTION_FAILED_INCORRECT_PIN));
  localized_strings->SetString("bluetoothErrorTimeout",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTION_FAILED_TIMEOUT));
  localized_strings->SetString("bluetoothErrorConnectionFailed",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTION_FAILED));
}

// TODO(kevers): Reorder methods to match ordering in the header file.

void BluetoothOptionsHandler::AdapterPresentChanged(BluetoothAdapter* adapter,
                                                    bool present) {
  DCHECK(adapter == adapter_.get());
  if (present) {
    web_ui()->CallJavascriptFunction(
        "options.BrowserOptions.showBluetoothSettings");

    // Update the checkbox and visibility based on the powered state of the
    // new adapter.
    AdapterPoweredChanged(adapter_.get(), adapter_->IsPowered());
  } else {
    web_ui()->CallJavascriptFunction(
        "options.BrowserOptions.hideBluetoothSettings");
  }
}

void BluetoothOptionsHandler::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                                    bool powered) {
  DCHECK(adapter == adapter_.get());
  base::FundamentalValue checked(powered);
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.setBluetoothState", checked);
}

void BluetoothOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initializeBluetoothStatus",
      base::Bind(&BluetoothOptionsHandler::InitializeBluetoothStatusCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("bluetoothEnableChange",
      base::Bind(&BluetoothOptionsHandler::EnableChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("findBluetoothDevices",
      base::Bind(&BluetoothOptionsHandler::FindDevicesCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("updateBluetoothDevice",
      base::Bind(&BluetoothOptionsHandler::UpdateDeviceCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("stopBluetoothDeviceDiscovery",
      base::Bind(&BluetoothOptionsHandler::StopDiscoveryCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPairedBluetoothDevices",
      base::Bind(&BluetoothOptionsHandler::GetPairedDevicesCallback,
                 base::Unretained(this)));
}

void BluetoothOptionsHandler::InitializeBluetoothStatusCallback(
    const ListValue* args) {
  // Bluetooth support is a work in progress.  Supress the feature unless
  // explicitly enabled via a command line flag.
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kEnableBluetooth)) {
    return;
  }
  adapter_.reset(BluetoothAdapter::CreateDefaultAdapter());
  adapter_->AddObserver(this);
  // Show or hide the bluetooth settings and update the checkbox based
  // on the current present/powered state.
  AdapterPresentChanged(adapter_.get(), adapter_->IsPresent());
}

void BluetoothOptionsHandler::EnableChangeCallback(
    const ListValue* args) {
  bool bluetooth_enabled;
  args->GetBoolean(0, &bluetooth_enabled);

  adapter_->SetPowered(bluetooth_enabled,
                       base::Bind(&BluetoothOptionsHandler::ErrorCallback,
                                  base::Unretained(this)));
}

void BluetoothOptionsHandler::FindDevicesCallback(
    const ListValue* args) {
  adapter_->SetDiscovering(true,
                           base::Bind(&BluetoothOptionsHandler::ErrorCallback,
                                      base::Unretained(this)));
}

void BluetoothOptionsHandler::UpdateDeviceCallback(
    const ListValue* args) {
  // TODO(kevers): Trigger connect/disconnect.
  int size = args->GetSize();
  std::string address;
  std::string command;
  args->GetString(kUpdateDeviceAddressIndex, &address);
  args->GetString(kUpdateDeviceCommandIndex, &command);
  if (size > kUpdateDevicePasskeyIndex) {
    // Passkey confirmation as part of the pairing process.
    std::string passkey;
    args->GetString(kUpdateDevicePasskeyIndex, &passkey);
    DVLOG(1) << "UpdateDeviceCallback: " << address << ": " << command
            << " [" << passkey << "]";
  } else {
    // Initiating a device connection or disconnecting
    DVLOG(1) << "UpdateDeviceCallback: " << address << ": " << command;
  }
}

void BluetoothOptionsHandler::StopDiscoveryCallback(
    const ListValue* args) {
  adapter_->SetDiscovering(false,
                           base::Bind(&BluetoothOptionsHandler::ErrorCallback,
                                      base::Unretained(this)));
}

void BluetoothOptionsHandler::GetPairedDevicesCallback(
    const ListValue* args) {
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();

  for (BluetoothAdapter::DeviceList::iterator iter = devices.begin();
       iter != devices.end(); ++iter)
    SendDeviceNotification(*iter, NULL);
}

void BluetoothOptionsHandler::SendDeviceNotification(
    const BluetoothDevice* device,
    base::DictionaryValue* params) {
  base::DictionaryValue js_properties;
  js_properties.SetString("name", device->GetName());
  js_properties.SetString("address", device->address());
  js_properties.SetBoolean("discovered", device->WasDiscovered());
  js_properties.SetBoolean("paired", device->IsPaired());
  js_properties.SetBoolean("connected", device->IsConnected());
  if (params) {
    js_properties.MergeDictionary(params);
  }
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.addBluetoothDevice",
      js_properties);
}

void BluetoothOptionsHandler::DisplayPinCode(
    const BluetoothDevice* device,
    const std::string& pincode) {
  DictionaryValue params;
  params.SetString("pairing", "bluetoothRemotePinCode");
  params.SetString("pincode", pincode);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::DisplayPasskey(
    const BluetoothDevice* device,
    int passkey,
    int entered) {
  DictionaryValue params;
  params.SetString("pairing", "bluetoothRemotePasskey");
  params.SetInteger("passkey", passkey);
  params.SetInteger("entered", entered);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::RequestPinCode(
    const BluetoothDevice* device) {
  DictionaryValue params;
  params.SetString("pairing", "bluetoothEnterPinCode");
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::RequestPasskey(
    const BluetoothDevice* device) {
  DictionaryValue params;
  params.SetString("pairing", "bluetoothEnterPasskey");
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::RequestConfirmation(
    const BluetoothDevice* device,
    int passkey) {
  DictionaryValue params;
  params.SetString("pairing", "bluetoothConfirmPasskey");
  params.SetInteger("passkey", passkey);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::ReportError(
    const BluetoothDevice* device,
    ConnectionError error) {
  std::string errorCode;
  switch (error) {
  case DEVICE_NOT_FOUND:
    errorCode = "bluetoothErrorNoDevice";
    break;
  case INCORRECT_PIN:
    errorCode = "bluetoothErrorIncorrectPin";
    break;
  case CONNECTION_TIMEOUT:
    errorCode = "bluetoothErrorTimeout";
    break;
  case CONNECTION_REJECTED:
    errorCode = "bluetoothErrorConnectionFailed";
    break;
  }
  DictionaryValue params;
  params.SetString("pairing", errorCode);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::DeviceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);
  SendDeviceNotification(device, NULL);
}

void BluetoothOptionsHandler::DeviceChanged(BluetoothAdapter* adapter,
                                            BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);
  SendDeviceNotification(device, NULL);
}

void BluetoothOptionsHandler::DeviceRemoved(BluetoothAdapter* adapter,
                                            BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);

  base::StringValue address(device->address());
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.removeBluetoothDevice",
      address);
}

void BluetoothOptionsHandler::ErrorCallback() {
  // TODO(keybuk): we don't get any form of error response from dbus::
  // yet, other than an error occurred. I'm going to fix that, then this
  // gets replaced by genuine error information from the method which we
  // can act on, rather than a debug log statement.
  DVLOG(1) << "Failed.";
}

}  // namespace options2
}  // namespace chromeos

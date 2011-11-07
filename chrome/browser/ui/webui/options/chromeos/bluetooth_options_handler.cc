// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/ui/webui/options/chromeos/system_settings_provider.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

BluetoothOptionsHandler::BluetoothOptionsHandler()
    : chromeos::CrosOptionsPageUIHandler(
        new chromeos::SystemSettingsProvider()) {
}

BluetoothOptionsHandler::~BluetoothOptionsHandler() {
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kEnableBluetooth)) {
    return;
  }

  chromeos::BluetoothManager* bluetooth_manager =
      chromeos::BluetoothManager::GetInstance();
  DCHECK(bluetooth_manager);

  chromeos::BluetoothAdapter* default_adapter =
      bluetooth_manager->DefaultAdapter();

  if (default_adapter != NULL) {
    default_adapter->RemoveObserver(this);
  }

  bluetooth_manager->RemoveObserver(this);
}

void BluetoothOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("bluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH));
  localized_strings->SetString("enableBluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_ENABLE));
  localized_strings->SetString("findBluetoothDevices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_FIND_BLUETOOTH_DEVICES));
  localized_strings->SetString("bluetoothScanning",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING));
  localized_strings->SetString("bluetoothDeviceConnected",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTED));
  localized_strings->SetString("bluetoothDeviceConnecting",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTING));
  localized_strings->SetString("bluetoothDeviceNotPaired",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_NOT_PAIRED));
  localized_strings->SetString("bluetoothDevicePaired",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_PAIRED));
  localized_strings->SetString("bluetoothDeviceFailedPairing",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_FAILED_PAIRING));
  localized_strings->SetString("bluetoothConnectDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT));
  localized_strings->SetString("bluetoothDisconnectDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT));
  localized_strings->SetString("bluetoothForgetDevice",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_FORGET));
  localized_strings->SetString("bluetoothCancel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_CANCEL));
  localized_strings->SetString("bluetoothConfirmPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothEnterPasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothRemotePasskey",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_REMOTE_PASSKEY_REQUEST));
  localized_strings->SetString("bluetoothFailedPairingInstructions",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_FAILED_PAIRING_INSTRUCTIONS));
}

void BluetoothOptionsHandler::Initialize() {
  DCHECK(web_ui_);
  // Bluetooth support is a work in progress.  Supress the feature unless
  // explicitly enabled via a command line flag.
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kEnableBluetooth)) {
    return;
  }

  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.showBluetoothSettings");

  // TODO(kevers): Determine whether bluetooth adapter is powered.
  bool bluetooth_on = true;
  base::FundamentalValue checked(bluetooth_on);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.setBluetoothCheckboxState", checked);

  chromeos::BluetoothManager* bluetooth_manager =
      chromeos::BluetoothManager::GetInstance();
  DCHECK(bluetooth_manager);
  bluetooth_manager->AddObserver(this);

  chromeos::BluetoothAdapter* default_adapter =
      bluetooth_manager->DefaultAdapter();
  DefaultAdapterChanged(default_adapter);
}

void BluetoothOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("bluetoothEnableChange",
      base::Bind(&BluetoothOptionsHandler::EnableChangeCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("findBluetoothDevices",
      base::Bind(&BluetoothOptionsHandler::FindDevicesCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("updateBluetoothDevice",
      base::Bind(&BluetoothOptionsHandler::UpdateDeviceCallback,
                 base::Unretained(this)));
}

void BluetoothOptionsHandler::EnableChangeCallback(
    const ListValue* args) {
  bool bluetooth_enabled;
  args->GetBoolean(0, &bluetooth_enabled);
  // TODO(kevers): Call Bluetooth API to enable or disable.
  base::FundamentalValue checked(bluetooth_enabled);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.setBluetoothCheckboxState", checked);
}

void BluetoothOptionsHandler::FindDevicesCallback(
    const ListValue* args) {
  // We only initiate a scan if we're running on Chrome OS. Otherwise, we
  // generate a fake device list.
  if (!chromeos::system::runtime_environment::IsRunningOnChromeOS()) {
    GenerateFakeDeviceList();
    return;
  }

  chromeos::BluetoothManager* bluetooth_manager =
      chromeos::BluetoothManager::GetInstance();
  DCHECK(bluetooth_manager);

  chromeos::BluetoothAdapter* default_adapter =
      bluetooth_manager->DefaultAdapter();

  ValidateDefaultAdapter(default_adapter);

  if (default_adapter == NULL) {
    VLOG(1) << "FindDevicesCallback: no default adapter";
    return;
  }

  default_adapter->StartDiscovery();
}

void BluetoothOptionsHandler::UpdateDeviceCallback(
    const ListValue* args) {
  // TODO(kevers): Trigger connect/disconnect.
}

void BluetoothOptionsHandler::DeviceNotification(
    const DictionaryValue& device) {
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.addBluetoothDevice", device);
}

void BluetoothOptionsHandler::RequestConfirmation(
    chromeos::BluetoothDevice* device,
    int passkey) {
  // TODO(kevers): Implement me.
}

void BluetoothOptionsHandler::DisplayPasskey(
    chromeos::BluetoothDevice* device,
    int passkey,
    int entered) {
  // TODO(kevers): Implement me.
}

void BluetoothOptionsHandler::RequestPasskey(
    chromeos::BluetoothDevice* device) {
  // TODO(kevers): Implement me.
}

void BluetoothOptionsHandler::ValidatePasskeyCallback(
    const base::ListValue* args) {
  // TODO(kevers): Implement me.
}

void BluetoothOptionsHandler::DefaultAdapterChanged(
    chromeos::BluetoothAdapter* adapter) {
  std::string old_default_adapter_id = default_adapter_id_;

  if (adapter == NULL) {
    default_adapter_id_.clear();
    VLOG(2) << "DefaultAdapterChanged: no default bluetooth adapter";
  } else {
    default_adapter_id_ = adapter->Id();
    VLOG(2) << "DefaultAdapterChanged: " << default_adapter_id_;
  }

  if (default_adapter_id_ == old_default_adapter_id) {
    return;
  }

  if (adapter != NULL) {
    adapter->AddObserver(this);
  }

  // TODO(vlaviano): Respond to adapter change.
}

void BluetoothOptionsHandler::DiscoveryStarted(const std::string& adapter_id) {
  VLOG(2) << "Discovery started on " << adapter_id;
}

void BluetoothOptionsHandler::DiscoveryEnded(const std::string& adapter_id) {
  VLOG(2) << "Discovery ended on " << adapter_id;
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.notifyBluetoothSearchComplete");

  // Stop the discovery session.
  // TODO(vlaviano): We may want to expose DeviceDisappeared, remove the
  // "Find devices" button, and let the discovery session continue throughout
  // the time that the page is visible rather than just doing a single discovery
  // cycle in response to a button click.
  chromeos::BluetoothManager* bluetooth_manager =
      chromeos::BluetoothManager::GetInstance();
  DCHECK(bluetooth_manager);

  chromeos::BluetoothAdapter* default_adapter =
      bluetooth_manager->DefaultAdapter();

  ValidateDefaultAdapter(default_adapter);

  if (default_adapter == NULL) {
    VLOG(1) << "DiscoveryEnded: no default adapter";
    return;
  }

  default_adapter->StopDiscovery();
}

void BluetoothOptionsHandler::DeviceFound(const std::string& adapter_id,
                                          chromeos::BluetoothDevice* device) {
  VLOG(2) << "Device found on " << adapter_id;
  DCHECK(device);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.addBluetoothDevice", device->AsDictionary());
}

void BluetoothOptionsHandler::ValidateDefaultAdapter(
    chromeos::BluetoothAdapter* adapter) {
  if ((adapter == NULL && !default_adapter_id_.empty()) ||
      (adapter != NULL && default_adapter_id_ != adapter->Id())) {
    VLOG(1) << "unexpected default adapter change from \""
            << default_adapter_id_ << "\" to \"" << adapter->Id() << "\"";
    DefaultAdapterChanged(adapter);
  }
}

void BluetoothOptionsHandler::GenerateFakeDeviceList() {
  GenerateFakeDiscoveredDevice(
    "Fake Wireless Keyboard",
    "01-02-03-04-05-06",
    "input-keyboard",
    true,
    true);
  GenerateFakeDiscoveredDevice(
    "Fake Wireless Mouse",
    "02-03-04-05-06-01",
    "input-mouse",
    true,
    false);
  GenerateFakeDiscoveredDevice(
    "Fake Wireless Headset",
    "03-04-05-06-01-02",
    "headset",
    false,
    false);
  GenerateFakePairing(
    "Fake Connecting Keyboard",
    "04-05-06-01-02-03",
    "input-keyboard",
    "bluetoothRemotePasskey");
  GenerateFakePairing(
    "Fake Connecting Phone",
    "05-06-01-02-03-04",
    "phone",
    "bluetoothConfirmPasskey");
  GenerateFakePairing(
    "Fake Connecting Headset",
    "06-01-02-03-04-05",
    "headset",
    "bluetoothEnterPasskey");

  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.notifyBluetoothSearchComplete");
}

void BluetoothOptionsHandler::GenerateFakeDiscoveredDevice(
    const std::string& name,
    const std::string& address,
    const std::string& icon,
    bool paired,
    bool connected) {
  DictionaryValue device;
  device.SetString("name", name);
  device.SetString("address", address);
  device.SetString("icon", icon);
  device.SetBoolean("paired", paired);
  device.SetBoolean("connected", connected);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.addBluetoothDevice", device);
}

void BluetoothOptionsHandler::GenerateFakePairing(
    const std::string& name,
    const std::string& address,
    const std::string& icon,
    const std::string& pairing) {
  DictionaryValue device;
  device.SetString("name", name);
  device.SetString("address", address);
  device.SetString("icon", icon);
  device.SetBoolean("paired", false);
  device.SetBoolean("connected", false);
  DictionaryValue op;
  op.SetString("pairing", pairing);
  if (pairing.compare("bluetoothEnterPasskey") != 0)
    op.SetInteger("passkey", 12345);
  if (pairing.compare("bluetoothRemotePasskey") == 0)
    op.SetInteger("entered", 2);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.connectBluetoothDevice", device, op);
}

}  // namespace chromeos


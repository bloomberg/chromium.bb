// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace options {

BluetoothOptionsHandler::BluetoothOptionsHandler() {
}

BluetoothOptionsHandler::~BluetoothOptionsHandler() {
}

void BluetoothOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "bluetooth", IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH },
    { "disableBluetooth", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISABLE },
    { "enableBluetooth", IDS_OPTIONS_SETTINGS_BLUETOOTH_ENABLE },
    { "addBluetoothDevice", IDS_OPTIONS_SETTINGS_ADD_BLUETOOTH_DEVICE },
    { "bluetoothAddDeviceTitle",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_ADD_DEVICE_TITLE },
    { "bluetoothOptionsPageTabTitle",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_ADD_DEVICE_TITLE },
    { "bluetoothNoDevices", IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES },
    { "bluetoothNoDevicesFound",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND },
    { "bluetoothScanning", IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING },
    { "bluetoothScanStopped", IDS_OPTIONS_SETTINGS_BLUETOOTH_SCAN_STOPPED },
    { "bluetoothDeviceConnecting", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTING },
    { "bluetoothConnectDevice", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT },
    { "bluetoothDisconnectDevice", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT },
    { "bluetoothForgetDevice", IDS_OPTIONS_SETTINGS_BLUETOOTH_FORGET },
    { "bluetoothCancel", IDS_OPTIONS_SETTINGS_BLUETOOTH_CANCEL },
    { "bluetoothEnterKey", IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_KEY },
    { "bluetoothDismissError", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISMISS_ERROR },

    // Device connecting and pairing.
    { "bluetoothStartConnecting",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_START_CONNECTING },
    { "bluetoothAcceptPasskey",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY },
    { "bluetoothRejectPasskey",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_REJECT_PASSKEY },
    { "bluetoothEnterPinCode",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_PIN_CODE_REQUEST },
    { "bluetoothEnterPasskey",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_PASSKEY_REQUEST },
    { "bluetoothRemotePinCode",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_REMOTE_PIN_CODE_REQUEST },
    { "bluetoothRemotePasskey",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_REMOTE_PASSKEY_REQUEST },
    { "bluetoothConfirmPasskey",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY_REQUEST },

    // Error messages.
    { "bluetoothStartDiscoveryFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_START_DISCOVERY_FAILED },
    { "bluetoothStopDiscoveryFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_STOP_DISCOVERY_FAILED },
    { "bluetoothChangePowerFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CHANGE_POWER_FAILED },
    { "bluetoothConnectUnknownError",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_UNKNOWN_ERROR },
    { "bluetoothConnectInProgress",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_IN_PROGRESS },
    { "bluetoothConnectFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_FAILED },
    { "bluetoothConnectAuthFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_AUTH_FAILED },
    { "bluetoothConnectAuthCanceled",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_AUTH_CANCELED },
    { "bluetoothConnectAuthRejected",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_AUTH_REJECTED },
    { "bluetoothConnectAuthTimeout",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_AUTH_TIMEOUT },
    { "bluetoothConnectUnsupportedDevice",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT_UNSUPPORTED_DEVICE },
    { "bluetoothDisconnectFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT_FAILED },
    { "bluetoothForgetFailed",
        IDS_OPTIONS_SETTINGS_BLUETOOTH_FORGET_FAILED }};

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

}  // namespace options
}  // namespace chromeos

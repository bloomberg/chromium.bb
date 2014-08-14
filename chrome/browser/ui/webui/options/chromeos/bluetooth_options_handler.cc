// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// |UpdateDeviceCallback| takes a variable length list as an argument. The
// value stored in each list element is indicated by the following constants.
const int kUpdateDeviceAddressIndex = 0;
const int kUpdateDeviceCommandIndex = 1;
const int kUpdateDeviceAuthTokenIndex = 2;

// |UpdateDeviceCallback| provides a command value of one of the following
// constants that indicates what update it is providing to us.
const char kConnectCommand[] = "connect";
const char kCancelCommand[] = "cancel";
const char kAcceptCommand[] = "accept";
const char kRejectCommand[] = "reject";
const char kDisconnectCommand[] = "disconnect";
const char kForgetCommand[] = "forget";

// |SendDeviceNotification| may include a pairing parameter whose value
// is one of the following constants instructing the UI to perform a certain
// action.
const char kStartConnecting[] = "bluetoothStartConnecting";
const char kEnterPinCode[] = "bluetoothEnterPinCode";
const char kEnterPasskey[] = "bluetoothEnterPasskey";
const char kRemotePinCode[] = "bluetoothRemotePinCode";
const char kRemotePasskey[] = "bluetoothRemotePasskey";
const char kConfirmPasskey[] = "bluetoothConfirmPasskey";

// An invalid |entered| value to represent the "undefined" value.
const int kInvalidEntered = 0xFFFF;

}  // namespace

namespace chromeos {
namespace options {

BluetoothOptionsHandler::BluetoothOptionsHandler()
    : should_run_device_discovery_(false),
      pairing_device_passkey_(1000000),
      pairing_device_entered_(kInvalidEntered),
      weak_ptr_factory_(this) {
}

BluetoothOptionsHandler::~BluetoothOptionsHandler() {
  if (adapter_.get())
    adapter_->RemoveObserver(this);
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
    { "findBluetoothDevices", IDS_OPTIONS_SETTINGS_FIND_BLUETOOTH_DEVICES },
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

// TODO(kevers): Reorder methods to match ordering in the header file.

void BluetoothOptionsHandler::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
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

void BluetoothOptionsHandler::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK(adapter == adapter_.get());
  base::FundamentalValue checked(powered);
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.setBluetoothState", checked);

  // If the "Add device" overlay is visible, dismiss it.
  if (!powered) {
    web_ui()->CallJavascriptFunction(
        "options.BluetoothOptions.dismissOverlay");
  }
}

void BluetoothOptionsHandler::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  DCHECK(adapter == adapter_.get());
  base::FundamentalValue discovering_value(discovering);
  web_ui()->CallJavascriptFunction(
      "options.BluetoothOptions.updateDiscoveryState", discovering_value);
}

void BluetoothOptionsHandler::RegisterMessages() {
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

void BluetoothOptionsHandler::InitializeHandler() {
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothOptionsHandler::InitializeAdapter,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothOptionsHandler::InitializePage() {
  // Show or hide the bluetooth settings and update the checkbox based
  // on the current present/powered state.
  AdapterPresentChanged(adapter_.get(), adapter_->IsPresent());

  // Automatically start device discovery if the "Add Bluetooth Device"
  // overlay is visible.
  web_ui()->CallJavascriptFunction(
      "options.BluetoothOptions.startDeviceDiscovery");
}

void BluetoothOptionsHandler::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_.get());
  adapter_->AddObserver(this);
}

void BluetoothOptionsHandler::EnableChangeCallback(
    const base::ListValue* args) {
  bool bluetooth_enabled;
  args->GetBoolean(0, &bluetooth_enabled);

  adapter_->SetPowered(bluetooth_enabled,
                       base::Bind(&base::DoNothing),
                       base::Bind(&BluetoothOptionsHandler::EnableChangeError,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothOptionsHandler::EnableChangeError() {
  VLOG(1) << "Failed to change power state.";
  ReportError("bluetoothChangePowerFailed", std::string());
}

void BluetoothOptionsHandler::FindDevicesCallback(
    const base::ListValue* args) {
  if (discovery_session_.get() && discovery_session_->IsActive()) {
    VLOG(1) << "Already have an active discovery session.";
    return;
  }
  should_run_device_discovery_ = true;
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothOptionsHandler::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothOptionsHandler::FindDevicesError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothOptionsHandler::OnStartDiscoverySession(
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the "Add Device" dialog was dismissed), don't claim the discovery
  // session and let it clean up.
  if (!should_run_device_discovery_)
    return;
  discovery_session_ = discovery_session.Pass();
}

void BluetoothOptionsHandler::FindDevicesError() {
  VLOG(1) << "Failed to start discovery.";
  ReportError("bluetoothStartDiscoveryFailed", std::string());
  if (!adapter_.get())
    return;
  base::FundamentalValue discovering(adapter_->IsDiscovering());
  web_ui()->CallJavascriptFunction(
      "options.BluetoothOptions.updateDiscoveryState", discovering);
}

void BluetoothOptionsHandler::UpdateDeviceCallback(
    const base::ListValue* args) {
  std::string address;
  args->GetString(kUpdateDeviceAddressIndex, &address);

  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device)
    return;

  std::string command;
  args->GetString(kUpdateDeviceCommandIndex, &command);

  if (command == kConnectCommand) {
    int size = args->GetSize();
    if (size > kUpdateDeviceAuthTokenIndex) {
      // PIN code or Passkey entry during the pairing process.
      std::string auth_token;
      args->GetString(kUpdateDeviceAuthTokenIndex, &auth_token);

      if (device->ExpectingPinCode()) {
        DeviceConnecting(device);
        // PIN Code is an array of 1 to 16 8-bit bytes, the usual
        // interpretation, and the one shared by BlueZ, is a UTF-8 string
        // of as many characters that will fit in that space, thus we
        // can use the auth token from JavaScript unmodified.
        VLOG(1) << "PIN Code supplied: " << address << ": " << auth_token;
        device->SetPinCode(auth_token);
      } else if (device->ExpectingPasskey()) {
        DeviceConnecting(device);
        // Passkey is a numeric in the range 0-999999, in this case the
        // JavaScript code should have ensured the auth token string only
        // contains digits so a simple conversion is sufficient. In the
        // failure case, just use 0 since that's the most likely Passkey
        // anyway, and if it's refused the device will request a new one.
        unsigned passkey = 0;
        base::StringToUint(auth_token, &passkey);

        VLOG(1) << "Passkey supplied: " << address << ": " << passkey;
        device->SetPasskey(passkey);
      } else {
        LOG(WARNING) << "Auth token supplied after pairing ended: " << address
                     << ": " << auth_token;
      }
    } else {
      // Determine if the device supports pairing:
      PairingDelegate* delegate = NULL;
      if (device->IsPairable())
        delegate = this;

      // Connection request.
      VLOG(1) << "Connect: " << address;
      device->Connect(
          delegate,
          base::Bind(&BluetoothOptionsHandler::Connected,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothOptionsHandler::ConnectError,
                     weak_ptr_factory_.GetWeakPtr(),
                     device->GetAddress()));
    }
  } else if (command == kCancelCommand) {
    // Cancel pairing.
    VLOG(1) << "Cancel pairing: " << address;
    device->CancelPairing();
  } else if (command == kAcceptCommand) {
    DeviceConnecting(device);
    // Confirm displayed Passkey.
    VLOG(1) << "Confirm pairing: " << address;
    device->ConfirmPairing();
  } else if (command == kRejectCommand) {
    // Reject displayed Passkey.
    VLOG(1) << "Reject pairing: " << address;
    device->RejectPairing();
  } else if (command == kDisconnectCommand) {
    // Disconnect from device.
    VLOG(1) << "Disconnect device: " << address;
    device->Disconnect(
        base::Bind(&base::DoNothing),
        base::Bind(&BluetoothOptionsHandler::DisconnectError,
                   weak_ptr_factory_.GetWeakPtr(),
                   device->GetAddress()));
  } else if (command == kForgetCommand) {
    // Disconnect from device and delete pairing information.
    VLOG(1) << "Forget device: " << address;
    device->Forget(base::Bind(&BluetoothOptionsHandler::ForgetError,
                              weak_ptr_factory_.GetWeakPtr(),
                              device->GetAddress()));
  } else {
    LOG(WARNING) << "Unknown updateBluetoothDevice command: " << command;
  }
}

void BluetoothOptionsHandler::Connected() {
  // Invalidate the local cache.
  pairing_device_address_.clear();
  pairing_device_entered_ = kInvalidEntered;

  web_ui()->CallJavascriptFunction(
      "options.BluetoothPairing.dismissDialog");
}

void BluetoothOptionsHandler::ConnectError(
    const std::string& address,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  const char* error_name = NULL;

  // Invalidate the local cache.
  pairing_device_address_.clear();
  pairing_device_entered_ = kInvalidEntered;

  VLOG(1) << "Failed to connect to device: " << address;
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      error_name = "bluetoothConnectUnknownError";
      break;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      error_name = "bluetoothConnectInProgress";
      break;
    case device::BluetoothDevice::ERROR_FAILED:
      error_name = "bluetoothConnectFailed";
      break;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      error_name = "bluetoothConnectAuthFailed";
      break;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      error_name = "bluetoothConnectAuthCanceled";
      break;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      error_name = "bluetoothConnectAuthRejected";
      break;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      error_name = "bluetoothConnectAuthTimeout";
      break;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      error_name = "bluetoothConnectUnsupportedDevice";
      break;
  }
  // Report an error only if there's an error to report.
  if (error_name)
    ReportError(error_name, address);
}

void BluetoothOptionsHandler::DisconnectError(const std::string& address) {
  VLOG(1) << "Failed to disconnect from device: " << address;
  ReportError("bluetoothDisconnectFailed", address);
}

void BluetoothOptionsHandler::ForgetError(const std::string& address) {
  VLOG(1) << "Failed to disconnect and unpair device: " << address;
  ReportError("bluetoothForgetFailed", address);
}

void BluetoothOptionsHandler::StopDiscoveryCallback(
    const base::ListValue* args) {
  should_run_device_discovery_ = false;
  if (!discovery_session_.get() || !discovery_session_->IsActive()) {
    VLOG(1) << "No active discovery session.";
    return;
  }
  discovery_session_->Stop(
      base::Bind(&base::DoNothing),
      base::Bind(&BluetoothOptionsHandler::StopDiscoveryError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothOptionsHandler::StopDiscoveryError() {
  VLOG(1) << "Failed to stop discovery.";
  ReportError("bluetoothStopDiscoveryFailed", std::string());
}

void BluetoothOptionsHandler::GetPairedDevicesCallback(
    const base::ListValue* args) {
  device::BluetoothAdapter::DeviceList devices = adapter_->GetDevices();

  for (device::BluetoothAdapter::DeviceList::iterator iter = devices.begin();
       iter != devices.end(); ++iter)
    SendDeviceNotification(*iter, NULL);
}

void BluetoothOptionsHandler::SendDeviceNotification(
    const device::BluetoothDevice* device,
    base::DictionaryValue* params) {
  base::DictionaryValue js_properties;
  js_properties.SetString("name", device->GetName());
  js_properties.SetString("address", device->GetAddress());
  js_properties.SetBoolean("paired", device->IsPaired());
  js_properties.SetBoolean("connected", device->IsConnected());
  js_properties.SetBoolean("connecting", device->IsConnecting());
  js_properties.SetBoolean("connectable", device->IsConnectable());
  if (params)
    js_properties.MergeDictionary(params);

  // Use the cached values to update js_property.
  if (device->GetAddress() == pairing_device_address_) {
    std::string pairing;
    if (!js_properties.GetString("pairing", &pairing)) {
      pairing = pairing_device_pairing_;
      js_properties.SetString("pairing", pairing);
    }
    if (pairing == kRemotePinCode && !js_properties.HasKey("pincode"))
      js_properties.SetString("pincode", pairing_device_pincode_);
    if (pairing == kRemotePasskey && !js_properties.HasKey("passkey"))
      js_properties.SetInteger("passkey", pairing_device_passkey_);
    if ((pairing == kRemotePinCode || pairing == kRemotePasskey) &&
        !js_properties.HasKey("entered") &&
        pairing_device_entered_ != kInvalidEntered) {
      js_properties.SetInteger("entered", pairing_device_entered_);
    }
  }

  // Update the cache with the new information.
  if (js_properties.HasKey("pairing")) {
    pairing_device_address_ = device->GetAddress();
    js_properties.GetString("pairing", &pairing_device_pairing_);
    js_properties.GetString("pincode", &pairing_device_pincode_);
    js_properties.GetInteger("passkey", &pairing_device_passkey_);
    if (!js_properties.GetInteger("entered", &pairing_device_entered_))
      pairing_device_entered_ = kInvalidEntered;
  }

  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.addBluetoothDevice",
      js_properties);
}

void BluetoothOptionsHandler::RequestPinCode(device::BluetoothDevice* device) {
  base::DictionaryValue params;
  params.SetString("pairing", kEnterPinCode);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::RequestPasskey(device::BluetoothDevice* device) {
  base::DictionaryValue params;
  params.SetString("pairing", kEnterPasskey);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::DisplayPinCode(device::BluetoothDevice* device,
                                             const std::string& pincode) {
  base::DictionaryValue params;
  params.SetString("pairing", kRemotePinCode);
  params.SetString("pincode", pincode);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::DisplayPasskey(device::BluetoothDevice* device,
                                             uint32 passkey) {
  base::DictionaryValue params;
  params.SetString("pairing", kRemotePasskey);
  params.SetInteger("passkey", passkey);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::KeysEntered(device::BluetoothDevice* device,
                                          uint32 entered) {
  base::DictionaryValue params;
  params.SetInteger("entered", entered);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::ConfirmPasskey(device::BluetoothDevice* device,
                                             uint32 passkey) {
  base::DictionaryValue params;
  params.SetString("pairing", kConfirmPasskey);
  params.SetInteger("passkey", passkey);
  SendDeviceNotification(device, &params);
}

void BluetoothOptionsHandler::AuthorizePairing(
    device::BluetoothDevice* device) {
  // There is never any circumstance where this will be called, since the
  // options handler will only be used for outgoing pairing requests, but
  // play it safe.
  device->ConfirmPairing();
}

void BluetoothOptionsHandler::ReportError(
    const std::string& error,
    const std::string& address) {
  base::DictionaryValue properties;
  properties.SetString("message", error);
  properties.SetString("address", address);
  web_ui()->CallJavascriptFunction(
      "options.BluetoothPairing.showMessage",
      properties);
}

void BluetoothOptionsHandler::DeviceAdded(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);
  SendDeviceNotification(device, NULL);
}

void BluetoothOptionsHandler::DeviceChanged(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);
  SendDeviceNotification(device, NULL);
}

void BluetoothOptionsHandler::DeviceRemoved(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  DCHECK(adapter == adapter_.get());
  DCHECK(device);

  // Invalidate the local cache if the pairing device is removed.
  if (pairing_device_address_ == device->GetAddress()) {
    pairing_device_address_.clear();
    pairing_device_entered_ = kInvalidEntered;
  }

  base::StringValue address(device->GetAddress());
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.removeBluetoothDevice",
      address);
}

void BluetoothOptionsHandler::DeviceConnecting(
    device::BluetoothDevice* device) {
  DCHECK(device);
  base::DictionaryValue params;
  params.SetString("pairing", kStartConnecting);
  SendDeviceNotification(device, &params);
}

}  // namespace options
}  // namespace chromeos

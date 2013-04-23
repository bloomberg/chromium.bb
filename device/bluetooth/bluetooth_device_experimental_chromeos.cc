// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_experimental_chromeos.h"

#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/experimental_bluetooth_adapter_client.h"
#include "chromeos/dbus/experimental_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/experimental_bluetooth_agent_service_provider.h"
#include "chromeos/dbus/experimental_bluetooth_device_client.h"
#include "chromeos/dbus/experimental_bluetooth_input_client.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluetooth_adapter_experimental_chromeos.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothDevice;

namespace {

// The agent path is relatively meaningless since BlueZ only supports one
// at time and will fail in an attempt to register another with "Already Exists"
// (which we fail in OnRegisterAgentError with ERROR_INPROGRESS).
const char kAgentPath[] = "/org/chromium/bluetooth_agent";

}  // namespace

namespace chromeos {

BluetoothDeviceExperimentalChromeOS::BluetoothDeviceExperimentalChromeOS(
    BluetoothAdapterExperimentalChromeOS* adapter,
    const dbus::ObjectPath& object_path)
    : adapter_(adapter),
      object_path_(object_path),
      num_connecting_calls_(0),
      pairing_delegate_(NULL),
      weak_ptr_factory_(this) {
}

BluetoothDeviceExperimentalChromeOS::~BluetoothDeviceExperimentalChromeOS() {
}

uint32 BluetoothDeviceExperimentalChromeOS::GetBluetoothClass() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->bluetooth_class.value();
}

std::string BluetoothDeviceExperimentalChromeOS::GetDeviceName() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

std::string BluetoothDeviceExperimentalChromeOS::GetAddress() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->address.value();
}

bool BluetoothDeviceExperimentalChromeOS::IsPaired() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  // Trusted devices are devices that don't support pairing but that the
  // user has explicitly connected; it makes no sense for UI purposes to
  // treat them differently from each other.
  return properties->paired.value() || properties->trusted.value();
}

bool BluetoothDeviceExperimentalChromeOS::IsConnected() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->connected.value();
}

bool BluetoothDeviceExperimentalChromeOS::IsConnectable() const {
  ExperimentalBluetoothInputClient::Properties* input_properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothInputClient()->
          GetProperties(object_path_);
  // GetProperties returns NULL when the device does not implement the given
  // interface. Non HID devices are normally connectable.
  if (!input_properties)
    return true;

  return input_properties->reconnect_mode.value() != "device";
}

bool BluetoothDeviceExperimentalChromeOS::IsConnecting() const {
  return num_connecting_calls_ > 0;
}

BluetoothDeviceExperimentalChromeOS::ServiceList
BluetoothDeviceExperimentalChromeOS::GetServices() const {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->uuids.value();
}

void BluetoothDeviceExperimentalChromeOS::GetServiceRecords(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback) {
  // TODO(keybuk): not implemented; remove
  error_callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::ProvidesServiceWithName(
    const std::string& name,
    const ProvidesServiceCallback& callback) {
  // TODO(keybuk): not implemented; remove
  callback.Run(false);
}

bool BluetoothDeviceExperimentalChromeOS::ExpectingPinCode() const {
  return !pincode_callback_.is_null();
}

bool BluetoothDeviceExperimentalChromeOS::ExpectingPasskey() const {
  return !passkey_callback_.is_null();
}

bool BluetoothDeviceExperimentalChromeOS::ExpectingConfirmation() const {
  return !confirmation_callback_.is_null();
}

void BluetoothDeviceExperimentalChromeOS::Connect(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  if (num_connecting_calls_++ == 0)
    adapter_->NotifyDeviceChanged(this);

  VLOG(1) << object_path_.value() << ": Connecting, " << num_connecting_calls_
          << " in progress";

  if (IsPaired() || IsConnected() || !pairing_delegate || !IsPairable()) {
    // No need to pair, or unable to, skip straight to connection.
    ConnectInternal(callback, error_callback);
  } else {
    // Initiate high-security connection with pairing.
    DCHECK(!pairing_delegate_);
    DCHECK(agent_.get() == NULL);

    pairing_delegate_ = pairing_delegate;

    // The agent path is relatively meaningless since BlueZ only supports
    // one per application at a time.
    dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
    agent_.reset(ExperimentalBluetoothAgentServiceProvider::Create(
        system_bus, dbus::ObjectPath(kAgentPath), this));
    DCHECK(agent_.get());

    VLOG(1) << object_path_.value() << ": Registering agent for pairing";
    DBusThreadManager::Get()->GetExperimentalBluetoothAgentManagerClient()->
        RegisterAgent(
            dbus::ObjectPath(kAgentPath),
            bluetooth_agent_manager::kKeyboardDisplayCapability,
            base::Bind(
                &BluetoothDeviceExperimentalChromeOS::OnRegisterAgent,
                weak_ptr_factory_.GetWeakPtr(),
                callback,
                error_callback),
            base::Bind(
                &BluetoothDeviceExperimentalChromeOS::OnRegisterAgentError,
                weak_ptr_factory_.GetWeakPtr(),
                error_callback));
  }
}

void BluetoothDeviceExperimentalChromeOS::SetPinCode(
    const std::string& pincode) {
  if (!agent_.get() || pincode_callback_.is_null())
    return;

  pincode_callback_.Run(SUCCESS, pincode);
  pincode_callback_.Reset();
}

void BluetoothDeviceExperimentalChromeOS::SetPasskey(uint32 passkey) {
  if (!agent_.get() || passkey_callback_.is_null())
    return;

  passkey_callback_.Run(SUCCESS, passkey);
  passkey_callback_.Reset();
}

void BluetoothDeviceExperimentalChromeOS::ConfirmPairing() {
  if (!agent_.get() || confirmation_callback_.is_null())
    return;

  confirmation_callback_.Run(SUCCESS);
  confirmation_callback_.Reset();
}

void BluetoothDeviceExperimentalChromeOS::RejectPairing() {
  RunPairingCallbacks(REJECTED);
}

void BluetoothDeviceExperimentalChromeOS::CancelPairing() {
  // If there wasn't a callback in progress that we can reply to then we
  // have to send a CancelPairing() to the device instead.
  if (!RunPairingCallbacks(CANCELLED)) {
    DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
        CancelPairing(
            object_path_,
            base::Bind(&base::DoNothing),
            base::Bind(
                &BluetoothDeviceExperimentalChromeOS::OnCancelPairingError,
                weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothDeviceExperimentalChromeOS::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Disconnecting";
  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      Disconnect(
          object_path_,
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnDisconnect,
              weak_ptr_factory_.GetWeakPtr(),
              callback),
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnDisconnectError,
              weak_ptr_factory_.GetWeakPtr(),
              error_callback));
}

void BluetoothDeviceExperimentalChromeOS::Forget(
   const ErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Removing device";
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      RemoveDevice(
          adapter_->object_path_,
          object_path_,
          base::Bind(&base::DoNothing),
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnForgetError,
              weak_ptr_factory_.GetWeakPtr(),
              error_callback));
}

void BluetoothDeviceExperimentalChromeOS::ConnectToService(
    const std::string& service_uuid,
    const SocketCallback& callback) {
  // TODO(keybuk): implement
  callback.Run(scoped_refptr<device::BluetoothSocket>());
}

void BluetoothDeviceExperimentalChromeOS::ConnectToProfile(
    device::BluetoothProfile* profile,
    const ErrorCallback& error_callback) {
  // TODO(keybuk): implement
  error_callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::SetOutOfBandPairingData(
    const device::BluetoothOutOfBandPairingData& data,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(keybuk): implement
  error_callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::ClearOutOfBandPairingData(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(keybuk): implement
  error_callback.Run();
}


void BluetoothDeviceExperimentalChromeOS::Release() {
  DCHECK(agent_.get());
  DCHECK(pairing_delegate_);
  VLOG(1) << object_path_.value() << ": Release";

  pincode_callback_.Reset();
  passkey_callback_.Reset();
  confirmation_callback_.Reset();

  UnregisterAgent();
}

void BluetoothDeviceExperimentalChromeOS::RequestPinCode(
    const dbus::ObjectPath& device_path,
    const PinCodeCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  VLOG(1) << object_path_.value() << ": RequestPinCode";

  DCHECK(pairing_delegate_);
  DCHECK(pincode_callback_.is_null());
  pincode_callback_ = callback;
  pairing_delegate_->RequestPinCode(this);
}

void BluetoothDeviceExperimentalChromeOS::DisplayPinCode(
    const dbus::ObjectPath& device_path,
    const std::string& pincode) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  VLOG(1) << object_path_.value() << ": DisplayPinCode: " << pincode;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DisplayPinCode(this, pincode);
}

void BluetoothDeviceExperimentalChromeOS::RequestPasskey(
    const dbus::ObjectPath& device_path,
    const PasskeyCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  VLOG(1) << object_path_.value() << ": RequestPasskey";

  DCHECK(pairing_delegate_);
  DCHECK(passkey_callback_.is_null());
  passkey_callback_ = callback;
  pairing_delegate_->RequestPasskey(this);
}

void BluetoothDeviceExperimentalChromeOS::DisplayPasskey(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    uint16 entered) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  VLOG(1) << object_path_.value() << ": DisplayPasskey: " << passkey
          << " (" << entered << " entered)";

  DCHECK(pairing_delegate_);
  if (entered == 0)
    pairing_delegate_->DisplayPasskey(this, passkey);
  pairing_delegate_->KeysEntered(this, entered);
}

void BluetoothDeviceExperimentalChromeOS::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  VLOG(1) << object_path_.value() << ": RequestConfirmation: " << passkey;

  DCHECK(pairing_delegate_);
  DCHECK(confirmation_callback_.is_null());
  confirmation_callback_ = callback;
  pairing_delegate_->ConfirmPasskey(this, passkey);
}

void BluetoothDeviceExperimentalChromeOS::RequestAuthorization(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  // TODO(keybuk): implement
  callback.Run(CANCELLED);
}

void BluetoothDeviceExperimentalChromeOS::AuthorizeService(
    const dbus::ObjectPath& device_path,
    const std::string& uuid,
    const ConfirmationCallback& callback) {
  // TODO(keybuk): implement
  callback.Run(CANCELLED);
}

void BluetoothDeviceExperimentalChromeOS::Cancel() {
  DCHECK(agent_.get());
  VLOG(1) << object_path_.value() << ": Cancel";

  DCHECK(pairing_delegate_);
  pairing_delegate_->DismissDisplayOrConfirm();
}

void BluetoothDeviceExperimentalChromeOS::ConnectInternal(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Connecting";
  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      Connect(
          object_path_,
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnConnect,
              weak_ptr_factory_.GetWeakPtr(),
              callback),
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnConnectError,
              weak_ptr_factory_.GetWeakPtr(),
              error_callback));
}

void BluetoothDeviceExperimentalChromeOS::OnConnect(
    const base::Closure& callback) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  VLOG(1) << object_path_.value() << ": Connected, " << num_connecting_calls_
        << " still in progress";

  SetTrusted();

  callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::OnConnectError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value() << ": Failed to connect device: "
               << error_name << ": " << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_adapter::kErrorFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorInProgress) {
    error_code = ERROR_INPROGRESS;
  } else if (error_name == bluetooth_adapter::kErrorNotSupported) {
    error_code = ERROR_UNSUPPORTED_DEVICE;
  }

  error_callback.Run(error_code);
}

void BluetoothDeviceExperimentalChromeOS::OnRegisterAgent(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Agent registered, now pairing";

  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      Pair(object_path_,
           base::Bind(
               &BluetoothDeviceExperimentalChromeOS::OnPair,
               weak_ptr_factory_.GetWeakPtr(),
               callback, error_callback),
           base::Bind(
               &BluetoothDeviceExperimentalChromeOS::OnPairError,
               weak_ptr_factory_.GetWeakPtr(),
               error_callback));
}

void BluetoothDeviceExperimentalChromeOS::OnRegisterAgentError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value() << ": Failed to register agent: "
               << error_name << ": " << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  UnregisterAgent();

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_adapter::kErrorAlreadyExists)
    error_code = ERROR_INPROGRESS;

  error_callback.Run(error_code);
}

void BluetoothDeviceExperimentalChromeOS::OnPair(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  VLOG(1) << object_path_.value() << ": Paired";
  UnregisterAgent();
  SetTrusted();
  ConnectInternal(callback, error_callback);
}

void BluetoothDeviceExperimentalChromeOS::OnPairError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  if (--num_connecting_calls_ == 0)
    adapter_->NotifyDeviceChanged(this);

  DCHECK(num_connecting_calls_ >= 0);
  LOG(WARNING) << object_path_.value() << ": Failed to pair device: "
               << error_name << ": " << error_message;
  VLOG(1) << object_path_.value() << ": " << num_connecting_calls_
          << " still in progress";

  UnregisterAgent();

  // Determine the error code from error_name.
  ConnectErrorCode error_code = ERROR_UNKNOWN;
  if (error_name == bluetooth_adapter::kErrorConnectionAttemptFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationFailed) {
    error_code = ERROR_AUTH_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationCanceled) {
    error_code = ERROR_AUTH_CANCELED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationRejected) {
    error_code = ERROR_AUTH_REJECTED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationTimeout) {
    error_code = ERROR_AUTH_TIMEOUT;
  }

  error_callback.Run(error_code);
}

void BluetoothDeviceExperimentalChromeOS::OnCancelPairingError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to cancel pairing: "
               << error_name << ": " << error_message;
}

void BluetoothDeviceExperimentalChromeOS::SetTrusted() {
  // Unconditionally send the property change, rather than checking the value
  // first; there's no harm in doing this and it solves any race conditions
  // with the property becoming true or false and this call happening before
  // we get the D-Bus signal about the earlier change.
  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      GetProperties(object_path_)->trusted.Set(
          true,
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnSetTrusted,
              weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceExperimentalChromeOS::OnSetTrusted(bool success) {
  LOG_IF(WARNING, !success) << object_path_.value()
                            << ": Failed to set device as trusted";
}

void BluetoothDeviceExperimentalChromeOS::UnregisterAgent() {
  DCHECK(agent_.get());
  DCHECK(pairing_delegate_);

  DCHECK(pincode_callback_.is_null());
  DCHECK(passkey_callback_.is_null());
  DCHECK(confirmation_callback_.is_null());

  pairing_delegate_->DismissDisplayOrConfirm();
  pairing_delegate_ = NULL;

  agent_.reset();

  // Clean up after ourselves.
  VLOG(1) << object_path_.value() << ": Unregistering pairing agent";
  DBusThreadManager::Get()->GetExperimentalBluetoothAgentManagerClient()->
      UnregisterAgent(
          dbus::ObjectPath(kAgentPath),
          base::Bind(&base::DoNothing),
          base::Bind(
              &BluetoothDeviceExperimentalChromeOS::OnUnregisterAgentError,
              weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceExperimentalChromeOS::OnUnregisterAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to unregister agent: "
               << error_name << ": " << error_message;
}

void BluetoothDeviceExperimentalChromeOS::OnDisconnect(
    const base::Closure& callback) {
  VLOG(1) << object_path_.value() << ": Disconnected";
  callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::OnDisconnectError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to disconnect device: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothDeviceExperimentalChromeOS::OnForgetError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to remove device: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

bool BluetoothDeviceExperimentalChromeOS::RunPairingCallbacks(Status status) {
  if (!agent_.get())
    return false;

  bool callback_run = false;
  if (!pincode_callback_.is_null()) {
    pincode_callback_.Run(status, "");
    pincode_callback_.Reset();
    callback_run = true;
  }

  if (!passkey_callback_.is_null()) {
    passkey_callback_.Run(status, 0);
    passkey_callback_.Reset();
    callback_run = true;
  }

  if (!confirmation_callback_.is_null()) {
    confirmation_callback_.Run(status);
    confirmation_callback_.Reset();
    callback_run = true;
  }

  return callback_run;
}

}  // namespace chromeos

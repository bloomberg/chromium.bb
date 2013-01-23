// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_chromeos.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/introspectable_client.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_service_record_chromeos.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothDevice;
using device::BluetoothOutOfBandPairingData;
using device::BluetoothServiceRecord;
using device::BluetoothSocket;

namespace chromeos {

BluetoothDeviceChromeOs::BluetoothDeviceChromeOs(
    BluetoothAdapterChromeOs* adapter)
  : BluetoothDevice(),
    adapter_(adapter),
    pairing_delegate_(NULL),
    connecting_applications_counter_(0),
    weak_ptr_factory_(this) {
}

BluetoothDeviceChromeOs::~BluetoothDeviceChromeOs() {
}

bool BluetoothDeviceChromeOs::IsPaired() const {
  return !object_path_.value().empty();
}

const BluetoothDevice::ServiceList&
BluetoothDeviceChromeOs::GetServices() const {
  return service_uuids_;
}

void BluetoothDeviceChromeOs::GetServiceRecords(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      DiscoverServices(
          object_path_,
          "",  // empty pattern to browse all services
          base::Bind(&BluetoothDeviceChromeOs::CollectServiceRecordsCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothDeviceChromeOs::ProvidesServiceWithUUID(
    const std::string& uuid) const {
  const BluetoothDevice::ServiceList& services = GetServices();
  for (BluetoothDevice::ServiceList::const_iterator iter = services.begin();
       iter != services.end();
       ++iter) {
    if (device::bluetooth_utils::CanonicalUuid(*iter) == uuid)
      return true;
  }
  return false;
}

void BluetoothDeviceChromeOs::ProvidesServiceWithName(
    const std::string& name,
    const ProvidesServiceCallback& callback) {
  GetServiceRecords(
      base::Bind(&BluetoothDeviceChromeOs::SearchServicesForNameCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 name,
                 callback),
      base::Bind(&BluetoothDeviceChromeOs::SearchServicesForNameErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

bool BluetoothDeviceChromeOs::ExpectingPinCode() const {
  return !pincode_callback_.is_null();
}

bool BluetoothDeviceChromeOs::ExpectingPasskey() const {
  return !passkey_callback_.is_null();
}

bool BluetoothDeviceChromeOs::ExpectingConfirmation() const {
  return !confirmation_callback_.is_null();
}

void BluetoothDeviceChromeOs::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  if (IsPaired() || IsBonded() || IsConnected()) {
    // Connection to already paired or connected device.
    ConnectApplications(callback, error_callback);

  } else if (!pairing_delegate) {
    // No pairing delegate supplied, initiate low-security connection only.
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->
        CreateDevice(adapter_->object_path_,
                     address_,
                     base::Bind(&BluetoothDeviceChromeOs::OnCreateDevice,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback,
                                error_callback),
                     base::Bind(&BluetoothDeviceChromeOs::OnCreateDeviceError,
                                weak_ptr_factory_.GetWeakPtr(),
                                error_callback));
  } else {
    // Initiate high-security connection with pairing.
    DCHECK(!pairing_delegate_);
    pairing_delegate_ = pairing_delegate;

    // The agent path is relatively meaningless, we use the device address
    // to generate it as we only support one pairing attempt at a time for
    // a given bluetooth device.
    DCHECK(agent_.get() == NULL);

    std::string agent_path_basename;
    ReplaceChars(address_, ":", "_", &agent_path_basename);
    dbus::ObjectPath agent_path("/org/chromium/bluetooth_agent/" +
                                agent_path_basename);

    dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
    if (system_bus) {
      agent_.reset(BluetoothAgentServiceProvider::Create(system_bus,
                                                         agent_path,
                                                         this));
    } else {
      agent_.reset(NULL);
    }

    DVLOG(1) << "Pairing: " << address_;
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->
        CreatePairedDevice(
            adapter_->object_path_,
            address_,
            agent_path,
            bluetooth_agent::kDisplayYesNoCapability,
            base::Bind(&BluetoothDeviceChromeOs::OnCreateDevice,
                       weak_ptr_factory_.GetWeakPtr(),
                       callback,
                       error_callback),
            base::Bind(&BluetoothDeviceChromeOs::OnCreateDeviceError,
                       weak_ptr_factory_.GetWeakPtr(),
                       error_callback));
  }
}

void BluetoothDeviceChromeOs::SetPinCode(const std::string& pincode) {
  if (!agent_.get() || pincode_callback_.is_null())
    return;

  pincode_callback_.Run(SUCCESS, pincode);
  pincode_callback_.Reset();
}

void BluetoothDeviceChromeOs::SetPasskey(uint32 passkey) {
  if (!agent_.get() || passkey_callback_.is_null())
    return;

  passkey_callback_.Run(SUCCESS, passkey);
  passkey_callback_.Reset();
}

void BluetoothDeviceChromeOs::ConfirmPairing() {
  if (!agent_.get() || confirmation_callback_.is_null())
    return;

  confirmation_callback_.Run(SUCCESS);
  confirmation_callback_.Reset();
}

void BluetoothDeviceChromeOs::RejectPairing() {
  if (!agent_.get())
    return;

  if (!pincode_callback_.is_null()) {
    pincode_callback_.Run(REJECTED, "");
    pincode_callback_.Reset();
  }
  if (!passkey_callback_.is_null()) {
    passkey_callback_.Run(REJECTED, 0);
    passkey_callback_.Reset();
  }
  if (!confirmation_callback_.is_null()) {
    confirmation_callback_.Run(REJECTED);
    confirmation_callback_.Reset();
  }
}

void BluetoothDeviceChromeOs::CancelPairing() {
  bool have_callback = false;
  if (agent_.get()) {
    if (!pincode_callback_.is_null()) {
      pincode_callback_.Run(CANCELLED, "");
      pincode_callback_.Reset();
      have_callback = true;
    }
    if (!passkey_callback_.is_null()) {
      passkey_callback_.Run(CANCELLED, 0);
      passkey_callback_.Reset();
      have_callback = true;
    }
    if (!confirmation_callback_.is_null()) {
      confirmation_callback_.Run(CANCELLED);
      confirmation_callback_.Reset();
      have_callback = true;
    }
  }

  if (!have_callback) {
    // User cancels the pairing process.
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->CancelDeviceCreation(
        adapter_->object_path_,
        address_,
        base::Bind(&BluetoothDeviceChromeOs::OnCancelDeviceCreation,
                   weak_ptr_factory_.GetWeakPtr()));

    pairing_delegate_ = NULL;
    agent_.reset();
  }
}

void BluetoothDeviceChromeOs::Disconnect(const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      Disconnect(object_path_,
                 base::Bind(&BluetoothDeviceChromeOs::DisconnectCallback,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback,
                            error_callback));

}

void BluetoothDeviceChromeOs::Forget(const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      RemoveDevice(adapter_->object_path_,
                   object_path_,
                   base::Bind(&BluetoothDeviceChromeOs::ForgetCallback,
                              weak_ptr_factory_.GetWeakPtr(),
                              error_callback));
}

void BluetoothDeviceChromeOs::ConnectToService(const std::string& service_uuid,
                                               const SocketCallback& callback) {
  GetServiceRecords(
      base::Bind(&BluetoothDeviceChromeOs::GetServiceRecordsForConnectCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_uuid,
                 callback),
      base::Bind(
          &BluetoothDeviceChromeOs::GetServiceRecordsForConnectErrorCallback,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

void BluetoothDeviceChromeOs::SetOutOfBandPairingData(
    const BluetoothOutOfBandPairingData& data,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      AddRemoteData(
          object_path_,
          address(),
          data,
          base::Bind(&BluetoothDeviceChromeOs::OnRemoteDataCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void BluetoothDeviceChromeOs::ClearOutOfBandPairingData(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      RemoveRemoteData(
          object_path_,
          address(),
          base::Bind(&BluetoothDeviceChromeOs::OnRemoteDataCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void BluetoothDeviceChromeOs::SetObjectPath(
    const dbus::ObjectPath& object_path) {
  DCHECK(object_path_ == dbus::ObjectPath(""));
  object_path_ = object_path;
}

void BluetoothDeviceChromeOs::RemoveObjectPath() {
  DCHECK(object_path_ != dbus::ObjectPath(""));
  object_path_ = dbus::ObjectPath("");
}

void BluetoothDeviceChromeOs::Update(
    const BluetoothDeviceClient::Properties* properties,
    bool update_state) {
  std::string address = properties->address.value();
  std::string name = properties->name.value();
  uint32 bluetooth_class = properties->bluetooth_class.value();
  const std::vector<std::string>& uuids = properties->uuids.value();

  if (!address.empty())
    address_ = address;
  if (!name.empty())
    name_ = name;
  if (bluetooth_class)
    bluetooth_class_ = bluetooth_class;
  if (!uuids.empty()) {
    service_uuids_.clear();
    service_uuids_.assign(uuids.begin(), uuids.end());
  }

  if (update_state) {
    // BlueZ uses paired to mean link keys exchanged, whereas the Bluetooth
    // spec refers to this as bonded. Use the spec name for our interface.
    bonded_ = properties->paired.value();
    connected_ = properties->connected.value();
  }
}

void BluetoothDeviceChromeOs::OnCreateDevice(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback,
    const dbus::ObjectPath& device_path) {
  DVLOG(1) << "Connection successful: " << device_path.value();
  if (object_path_.value().empty()) {
    object_path_ = device_path;
  } else {
    LOG_IF(WARNING, object_path_ != device_path)
        << "Conflicting device paths for objects, result gave: "
        << device_path.value() << " but signal gave: "
        << object_path_.value();
  }

  // Mark the device trusted so it can connect to us automatically, and
  // we can connect after rebooting. This information is part of the
  // pairing information of the device, and is unique to the combination
  // of our bluetooth address and the device's bluetooth address. A
  // different host needs a new pairing, so it's not useful to sync.
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      GetProperties(object_path_)->trusted.Set(
          true,
          base::Bind(&BluetoothDeviceChromeOs::OnSetTrusted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     base::Bind(error_callback,
                                ERROR_UNKNOWN)));
  // TODO(deymo): Replace ERROR_UNKNOWN with an appropriate new constant.

  // Connect application-layer protocols.
  ConnectApplications(callback, error_callback);
}

void BluetoothDeviceChromeOs::OnCreateDeviceError(
    const ConnectErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  // The default |error_code| is an unknown error.
  ConnectErrorCode error_code = ERROR_UNKNOWN;

  // Report any error in the log, even if we know the possible source of it.
  LOG(WARNING) << "Connection failed (on CreatePairedDevice): "
               << "\"" << name_ << "\" (" << address_ << "): "
               << error_name << ": \"" << error_message << "\"";

  // Determines the right error code from error_name, assuming the error name
  // comes from CreatePairedDevice bluez function.
  if (error_name == bluetooth_adapter::kErrorConnectionAttemptFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationFailed) {
    error_code = ERROR_AUTH_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationRejected) {
    error_code = ERROR_AUTH_REJECTED;
  } else if (error_name == bluetooth_adapter::kErrorAuthenticationTimeout) {
    error_code = ERROR_AUTH_TIMEOUT;
  }
  error_callback.Run(error_code);
}

void BluetoothDeviceChromeOs::CollectServiceRecordsCallback(
    const ServiceRecordsCallback& callback,
    const ErrorCallback& error_callback,
    const dbus::ObjectPath& device_path,
    const BluetoothDeviceClient::ServiceMap& service_map,
    bool success) {
  if (!success) {
    error_callback.Run();
    return;
  }

  ScopedVector<BluetoothServiceRecord> records;
  for (BluetoothDeviceClient::ServiceMap::const_iterator i =
      service_map.begin(); i != service_map.end(); ++i) {
    records.push_back(
        new BluetoothServiceRecordChromeOS(address(), i->second));
  }
  callback.Run(records);
}

void BluetoothDeviceChromeOs::OnSetTrusted(const base::Closure& callback,
                                           const ErrorCallback& error_callback,
                                           bool success) {
  if (success) {
    callback.Run();
  } else {
    LOG(WARNING) << "Failed to set device as trusted: " << address_;
    error_callback.Run();
  }
}

void BluetoothDeviceChromeOs::ConnectApplications(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  // Introspect the device object to determine supported applications.
  DBusThreadManager::Get()->GetIntrospectableClient()->
      Introspect(bluetooth_device::kBluetoothDeviceServiceName,
                 object_path_,
                 base::Bind(&BluetoothDeviceChromeOs::OnIntrospect,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback,
                            error_callback));
}

void BluetoothDeviceChromeOs::OnIntrospect(
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback,
    const std::string& service_name,
    const dbus::ObjectPath& device_path,
    const std::string& xml_data,
    bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to determine supported applications: " << address_;
    error_callback.Run(ERROR_UNKNOWN);
    return;
  }

  // The introspection data for the device object may list one or more
  // additional D-Bus interfaces that BlueZ supports for this particular
  // device. Send appropraite Connect calls for each of those interfaces
  // to connect all of the application protocols for this device.
  std::vector<std::string> interfaces =
      IntrospectableClient::GetInterfacesFromIntrospectResult(xml_data);

  DCHECK_EQ(0, connecting_applications_counter_);
  connecting_applications_counter_ = 0;
  for (std::vector<std::string>::iterator iter = interfaces.begin();
       iter != interfaces.end(); ++iter) {
    if (*iter == bluetooth_input::kBluetoothInputInterface) {
      connecting_applications_counter_++;
      // Supports Input interface.
      DBusThreadManager::Get()->GetBluetoothInputClient()->
          Connect(object_path_,
                  base::Bind(&BluetoothDeviceChromeOs::OnConnect,
                             weak_ptr_factory_.GetWeakPtr(),
                             callback,
                             *iter),
                  base::Bind(&BluetoothDeviceChromeOs::OnConnectError,
                             weak_ptr_factory_.GetWeakPtr(),
                             error_callback, *iter));
    }
  }

  // If OnConnect has been called for every call to Connect above, then this
  // will decrement the counter to -1.  In that case, call the callback
  // directly as it has not been called by any of the OnConnect callbacks.
  // This is safe because OnIntrospect and OnConnect run on the same thread.
  connecting_applications_counter_--;
  if (connecting_applications_counter_ == -1)
    callback.Run();
}

void BluetoothDeviceChromeOs::OnConnect(const base::Closure& callback,
                                        const std::string& interface_name,
                                        const dbus::ObjectPath& device_path) {
  DVLOG(1) << "Application connection successful: " << device_path.value()
           << ": " << interface_name;

  connecting_applications_counter_--;
  // |callback| should only be called once, meaning it cannot be called before
  // all requests have been started.  The extra decrement after all requests
  // have been started, and the check for -1 instead of 0 below, insure only a
  // single call to |callback| will occur (provided OnConnect and OnIntrospect
  // run on the same thread, which is true).
  if (connecting_applications_counter_ == -1) {
    connecting_applications_counter_ = 0;
    callback.Run();
  }
}

void BluetoothDeviceChromeOs::OnConnectError(
    const ConnectErrorCallback& error_callback,
    const std::string& interface_name,
    const dbus::ObjectPath& device_path,
    const std::string& error_name,
    const std::string& error_message) {
  // The default |error_code| is an unknown error.
  ConnectErrorCode error_code = ERROR_UNKNOWN;

  // Report any error in the log, even if we know the possible source of it.
  LOG(WARNING) << "Connection failed (on Connect): "
               << interface_name << ": "
               << "\"" << name_ << "\" (" << address_ << "): "
               << error_name << ": \"" << error_message << "\"";

  // Determines the right error code from error_name, assuming the error name
  // comes from Connect bluez function.
  if (error_name == bluetooth_adapter::kErrorFailed) {
    error_code = ERROR_FAILED;
  } else if (error_name == bluetooth_adapter::kErrorInProgress) {
    error_code = ERROR_INPROGRESS;
  } else if (error_name == bluetooth_adapter::kErrorNotSupported) {
    error_code = ERROR_UNSUPPORTED_DEVICE;
  }

  error_callback.Run(error_code);
}

void BluetoothDeviceChromeOs::DisconnectCallback(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    const dbus::ObjectPath& device_path,
    bool success) {
  DCHECK(device_path == object_path_);
  if (success) {
    DVLOG(1) << "Disconnection successful: " << address_;
    callback.Run();
  } else {
    LOG(WARNING) << "Disconnection failed: " << address_;
    error_callback.Run();
  }
}

void BluetoothDeviceChromeOs::ForgetCallback(
    const ErrorCallback& error_callback,
    const dbus::ObjectPath& adapter_path,
    bool success) {
  // It's quite normal that this path never gets called on success; we use a
  // weak pointer, and bluetoothd might send the DeviceRemoved signal before
  // the method reply, in which case this object is deleted and the
  // callback never takes place. Therefore don't do anything here for the
  // success case.
  if (!success) {
    LOG(WARNING) << "Forget failed: " << address_;
    error_callback.Run();
  }
}

void BluetoothDeviceChromeOs::OnCancelDeviceCreation(
    const dbus::ObjectPath& adapter_path,
    bool success) {
  if (!success)
    LOG(WARNING) << "CancelDeviceCreation failed: " << address_;
}

void BluetoothDeviceChromeOs::SearchServicesForNameErrorCallback(
    const ProvidesServiceCallback& callback) {
  callback.Run(false);
}

void BluetoothDeviceChromeOs::SearchServicesForNameCallback(
    const std::string& name,
    const ProvidesServiceCallback& callback,
    const ServiceRecordList& list) {
  for (ServiceRecordList::const_iterator i = list.begin();
      i != list.end(); ++i) {
    if ((*i)->name() == name) {
      callback.Run(true);
      return;
    }
  }
  callback.Run(false);
}

void BluetoothDeviceChromeOs::GetServiceRecordsForConnectErrorCallback(
    const SocketCallback& callback) {
  callback.Run(NULL);
}

void BluetoothDeviceChromeOs::GetServiceRecordsForConnectCallback(
    const std::string& service_uuid,
    const SocketCallback& callback,
    const ServiceRecordList& list) {
  for (ServiceRecordList::const_iterator i = list.begin();
      i != list.end(); ++i) {
    if ((*i)->uuid() == service_uuid) {
      // If multiple service records are found, use the first one that works.
      scoped_refptr<BluetoothSocket> socket(
          BluetoothSocketChromeOs::CreateBluetoothSocket(**i));
      if (socket.get() != NULL) {
        callback.Run(socket);
        return;
      }
    }
  }
  callback.Run(NULL);
}

void BluetoothDeviceChromeOs::OnRemoteDataCallback(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothDeviceChromeOs::DisconnectRequested(
    const dbus::ObjectPath& object_path) {
  DCHECK(object_path == object_path_);
}

void BluetoothDeviceChromeOs::Release() {
  DCHECK(agent_.get());
  DVLOG(1) << "Release: " << address_;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DismissDisplayOrConfirm();
  pairing_delegate_ = NULL;

  pincode_callback_.Reset();
  passkey_callback_.Reset();
  confirmation_callback_.Reset();

  agent_.reset();
}

void BluetoothDeviceChromeOs::RequestPinCode(
    const dbus::ObjectPath& device_path,
    const PinCodeCallback& callback) {
  DCHECK(agent_.get());
  DVLOG(1) << "RequestPinCode: " << device_path.value();

  DCHECK(pairing_delegate_);
  DCHECK(pincode_callback_.is_null());
  pincode_callback_ = callback;
  pairing_delegate_->RequestPinCode(this);
}

void BluetoothDeviceChromeOs::RequestPasskey(
    const dbus::ObjectPath& device_path,
    const PasskeyCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "RequestPasskey: " << device_path.value();

  DCHECK(pairing_delegate_);
  DCHECK(passkey_callback_.is_null());
  passkey_callback_ = callback;
  pairing_delegate_->RequestPasskey(this);
}

void BluetoothDeviceChromeOs::DisplayPinCode(
    const dbus::ObjectPath& device_path,
    const std::string& pincode) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "DisplayPinCode: " << device_path.value() << " " << pincode;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DisplayPinCode(this, pincode);
}

void BluetoothDeviceChromeOs::DisplayPasskey(
    const dbus::ObjectPath& device_path,
    uint32 passkey) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "DisplayPasskey: " << device_path.value() << " " << passkey;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DisplayPasskey(this, passkey);
}

void BluetoothDeviceChromeOs::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "RequestConfirmation: " << device_path.value() << " " << passkey;

  DCHECK(pairing_delegate_);
  DCHECK(confirmation_callback_.is_null());
  confirmation_callback_ = callback;
  pairing_delegate_->ConfirmPasskey(this, passkey);
}

void BluetoothDeviceChromeOs::Authorize(const dbus::ObjectPath& device_path,
                                        const std::string& uuid,
                                        const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  LOG(WARNING) << "Rejected authorization for service: " << uuid
               << " requested from device: " << device_path.value();
  callback.Run(REJECTED);
}

void BluetoothDeviceChromeOs::ConfirmModeChange(
    Mode mode,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  LOG(WARNING) << "Rejected adapter-level mode change: " << mode
               << " made on agent for device: " << address_;
  callback.Run(REJECTED);
}

void BluetoothDeviceChromeOs::Cancel() {
  DCHECK(agent_.get());
  DVLOG(1) << "Cancel: " << address_;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DismissDisplayOrConfirm();
}


// static
BluetoothDeviceChromeOs* BluetoothDeviceChromeOs::Create(
    BluetoothAdapterChromeOs* adapter) {
  return new BluetoothDeviceChromeOs(adapter);
}

}  // namespace chromeos

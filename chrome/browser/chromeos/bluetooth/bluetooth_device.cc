// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"

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
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/introspectable_client.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
  : adapter_(adapter),
    bluetooth_class_(0),
    visible_(false),
    bonded_(false),
    connected_(false),
    pairing_delegate_(NULL),
    connecting_applications_counter_(0),
    weak_ptr_factory_(this) {
}

BluetoothDevice::~BluetoothDevice() {
}

const std::string& BluetoothDevice::address() const { return address_; }

string16 BluetoothDevice::GetName() const {
  if (!name_.empty()) {
    return UTF8ToUTF16(name_);
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

BluetoothDevice::DeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  switch ((bluetooth_class_ & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return DEVICE_COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class_ & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return DEVICE_PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return DEVICE_MODEM;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class_ & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class_ & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return DEVICE_JOYSTICK;
            case 0x02:
              // Gamepad.
              return DEVICE_GAMEPAD;
            default:
              return DEVICE_PERIPHERAL;
          }
          break;
        case 0x01:
          // Keyboard.
          return DEVICE_KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class_ & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return DEVICE_TABLET;
            default:
              // Mouse.
              return DEVICE_MOUSE;
          }
          break;
        case 0x03:
          // Combo device.
          return DEVICE_KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  return DEVICE_UNKNOWN;
}

string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  string16 address = UTF8ToUTF16(address_);
  DeviceType device_type = GetDeviceType();
  switch (device_type) {
    case DEVICE_COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address);
    case DEVICE_PHONE:
          return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                            address);
    case DEVICE_MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address);
    case DEVICE_JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address);
    case DEVICE_GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address);
    case DEVICE_KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address);
    case DEVICE_MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address);
    case DEVICE_TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address);
    case DEVICE_KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN, address);
  }
}

bool BluetoothDevice::IsSupported() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUnsupportedBluetoothDevices))
    return true;

  DeviceType device_type = GetDeviceType();
  return (device_type == DEVICE_JOYSTICK ||
          device_type == DEVICE_GAMEPAD ||
          device_type == DEVICE_KEYBOARD ||
          device_type == DEVICE_MOUSE ||
          device_type == DEVICE_TABLET ||
          device_type == DEVICE_KEYBOARD_MOUSE_COMBO);
}

bool BluetoothDevice::IsPaired() const { return !object_path_.value().empty(); }

bool BluetoothDevice::IsBonded() const { return bonded_; }

bool BluetoothDevice::IsConnected() const {
  // TODO(keybuk): examine protocol-specific connected state, such as Input
  return connected_;
}

void BluetoothDevice::GetServiceRecords(const ServiceRecordsCallback& callback,
                                        const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      DiscoverServices(
          object_path_,
          "",  // empty pattern to browse all services
          base::Bind(&BluetoothDevice::CollectServiceRecordsCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothDevice::ProvidesServiceWithUUID(const std::string& uuid) const {
  const BluetoothDevice::ServiceList& services = GetServices();
  for (BluetoothDevice::ServiceList::const_iterator iter = services.begin();
      iter != services.end(); ++iter) {
    if (bluetooth_utils::CanonicalUuid(*iter) == uuid)
      return true;
  }
  return false;
}

void BluetoothDevice::ProvidesServiceWithName(const std::string& name,
    const ProvidesServiceCallback& callback) {
  GetServiceRecords(
      base::Bind(&BluetoothDevice::SearchServicesForNameCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 name,
                 callback),
      base::Bind(&BluetoothDevice::SearchServicesForNameErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void BluetoothDevice::Connect(PairingDelegate* pairing_delegate,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) {
  if (IsPaired() || IsBonded() || IsConnected()) {
    // Connection to already paired or connected device.
    ConnectApplications(callback, error_callback);

  } else if (!pairing_delegate) {
    // No pairing delegate supplied, initiate low-security connection only.
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->
        CreateDevice(adapter_->object_path_,
                     address_,
                     base::Bind(&BluetoothDevice::ConnectCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback,
                                error_callback),
                     base::Bind(&BluetoothDevice::ConnectErrorCallback,
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
        CreatePairedDevice(adapter_->object_path_,
                           address_,
                           agent_path,
                           bluetooth_agent::kDisplayYesNoCapability,
                           base::Bind(&BluetoothDevice::ConnectCallback,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      callback,
                                      error_callback),
                           base::Bind(&BluetoothDevice::ConnectErrorCallback,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      error_callback));
  }
}

void BluetoothDevice::SetPinCode(const std::string& pincode) {
  if (!agent_.get() || pincode_callback_.is_null())
    return;

  pincode_callback_.Run(SUCCESS, pincode);
  pincode_callback_.Reset();
}

void BluetoothDevice::SetPasskey(uint32 passkey) {
  if (!agent_.get() || passkey_callback_.is_null())
    return;

  passkey_callback_.Run(SUCCESS, passkey);
  passkey_callback_.Reset();
}

void BluetoothDevice::ConfirmPairing() {
  if (!agent_.get() || confirmation_callback_.is_null())
    return;

  confirmation_callback_.Run(SUCCESS);
  confirmation_callback_.Reset();
}

void BluetoothDevice::RejectPairing() {
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

void BluetoothDevice::CancelPairing() {
  if (!agent_.get())
    return;

  if (!pincode_callback_.is_null()) {
    pincode_callback_.Run(CANCELLED, "");
    pincode_callback_.Reset();
  }
  if (!passkey_callback_.is_null()) {
    passkey_callback_.Run(CANCELLED, 0);
    passkey_callback_.Reset();
  }
  if (!confirmation_callback_.is_null()) {
    confirmation_callback_.Run(CANCELLED);
    confirmation_callback_.Reset();
  }
}

void BluetoothDevice::Disconnect(const base::Closure& callback,
                                 const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      Disconnect(object_path_,
                 base::Bind(&BluetoothDevice::DisconnectCallback,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback,
                            error_callback));

}

void BluetoothDevice::Forget(const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      RemoveDevice(adapter_->object_path_,
                   object_path_,
                   base::Bind(&BluetoothDevice::ForgetCallback,
                              weak_ptr_factory_.GetWeakPtr(),
                              error_callback));
}

void BluetoothDevice::ConnectToService(const std::string& service_uuid,
                                       const SocketCallback& callback) {
  GetServiceRecords(
      base::Bind(&BluetoothDevice::GetServiceRecordsForConnectCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_uuid,
                 callback),
      base::Bind(&BluetoothDevice::GetServiceRecordsForConnectErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void BluetoothDevice::SetOutOfBandPairingData(
    const chromeos::BluetoothOutOfBandPairingData& data,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      AddRemoteData(
          object_path_,
          address(),
          data,
          base::Bind(&BluetoothDevice::OnRemoteDataCallback,
              weak_ptr_factory_.GetWeakPtr(),
              callback,
              error_callback));
}

void BluetoothDevice::ClearOutOfBandPairingData(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      RemoveRemoteData(
          object_path_,
          address(),
          base::Bind(&BluetoothDevice::OnRemoteDataCallback,
              weak_ptr_factory_.GetWeakPtr(),
              callback,
              error_callback));
}

void BluetoothDevice::SetObjectPath(const dbus::ObjectPath& object_path) {
  DCHECK(object_path_ == dbus::ObjectPath(""));
  object_path_ = object_path;
}

void BluetoothDevice::RemoveObjectPath() {
  DCHECK(object_path_ != dbus::ObjectPath(""));
  object_path_ = dbus::ObjectPath("");
}

void BluetoothDevice::Update(
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

void BluetoothDevice::ConnectCallback(const base::Closure& callback,
                                      const ErrorCallback& error_callback,
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
          base::Bind(&BluetoothDevice::OnSetTrusted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));

  // Connect application-layer protocols.
  ConnectApplications(callback, error_callback);
}

void BluetoothDevice::ConnectErrorCallback(const ErrorCallback& error_callback,
                                           const std::string& error_name,
                                           const std::string& error_message) {
  LOG(WARNING) << "Connection failed: " << address_
               << ": " << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothDevice::CollectServiceRecordsCallback(
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
        new BluetoothServiceRecord(address(), i->second));
  }
  callback.Run(records);
}

void BluetoothDevice::OnSetTrusted(const base::Closure& callback,
                                   const ErrorCallback& error_callback,
                                   bool success) {
  if (success) {
    callback.Run();
  } else {
    LOG(WARNING) << "Failed to set device as trusted: " << address_;
    error_callback.Run();
  }
}

void BluetoothDevice::ConnectApplications(const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
  // Introspect the device object to determine supported applications.
  DBusThreadManager::Get()->GetIntrospectableClient()->
      Introspect(bluetooth_device::kBluetoothDeviceServiceName,
                 object_path_,
                 base::Bind(&BluetoothDevice::OnIntrospect,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback,
                            error_callback));
}

void BluetoothDevice::OnIntrospect(const base::Closure& callback,
                                   const ErrorCallback& error_callback,
                                   const std::string& service_name,
                                   const dbus::ObjectPath& device_path,
                                   const std::string& xml_data,
                                   bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to determine supported applications: " << address_;
    error_callback.Run();
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
                  base::Bind(&BluetoothDevice::OnConnect,
                             weak_ptr_factory_.GetWeakPtr(),
                             callback,
                             *iter),
                  base::Bind(&BluetoothDevice::OnConnectError,
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

void BluetoothDevice::OnConnect(const base::Closure& callback,
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

void BluetoothDevice::OnConnectError(const ErrorCallback& error_callback,
                                     const std::string& interface_name,
                                     const dbus::ObjectPath& device_path,
                                     const std::string& error_name,
                                     const std::string& error_message) {
  LOG(WARNING) << "Connection failed: " << address_ << ": " << interface_name
               << ": " << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothDevice::DisconnectCallback(const base::Closure& callback,
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

void BluetoothDevice::ForgetCallback(const ErrorCallback& error_callback,
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

void BluetoothDevice::SearchServicesForNameErrorCallback(
    const ProvidesServiceCallback& callback) {
  callback.Run(false);
}

void BluetoothDevice::SearchServicesForNameCallback(
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

void BluetoothDevice::GetServiceRecordsForConnectErrorCallback(
    const SocketCallback& callback) {
  callback.Run(NULL);
}

void BluetoothDevice::GetServiceRecordsForConnectCallback(
    const std::string& service_uuid,
    const SocketCallback& callback,
    const ServiceRecordList& list) {
  for (ServiceRecordList::const_iterator i = list.begin();
      i != list.end(); ++i) {
    if ((*i)->uuid() == service_uuid) {
      // If multiple service records are found, use the first one that works.
      scoped_refptr<BluetoothSocket> socket(
          BluetoothSocket::CreateBluetoothSocket(**i));
      if (socket.get() != NULL) {
        callback.Run(socket);
        return;
      }
    }
  }
  callback.Run(NULL);
}

void BluetoothDevice::OnRemoteDataCallback(const base::Closure& callback,
                                           const ErrorCallback& error_callback,
                                           bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothDevice::DisconnectRequested(const dbus::ObjectPath& object_path) {
  DCHECK(object_path == object_path_);
}

void BluetoothDevice::Release() {
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

void BluetoothDevice::RequestPinCode(const dbus::ObjectPath& device_path,
                                     const PinCodeCallback& callback) {
  DCHECK(agent_.get());
  DVLOG(1) << "RequestPinCode: " << device_path.value();

  DCHECK(pairing_delegate_);
  DCHECK(pincode_callback_.is_null());
  pincode_callback_ = callback;
  pairing_delegate_->RequestPinCode(this);
}

void BluetoothDevice::RequestPasskey(const dbus::ObjectPath& device_path,
                                     const PasskeyCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "RequestPasskey: " << device_path.value();

  DCHECK(pairing_delegate_);
  DCHECK(passkey_callback_.is_null());
  passkey_callback_ = callback;
  pairing_delegate_->RequestPasskey(this);
}

void BluetoothDevice::DisplayPinCode(const dbus::ObjectPath& device_path,
                                     const std::string& pincode) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "DisplayPinCode: " << device_path.value() << " " << pincode;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DisplayPinCode(this, pincode);
}

void BluetoothDevice::DisplayPasskey(const dbus::ObjectPath& device_path,
                                     uint32 passkey) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  DVLOG(1) << "DisplayPasskey: " << device_path.value() << " " << passkey;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DisplayPasskey(this, passkey);
}

void BluetoothDevice::RequestConfirmation(
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

void BluetoothDevice::Authorize(const dbus::ObjectPath& device_path,
                                const std::string& uuid,
                                const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  DCHECK(device_path == object_path_);
  LOG(WARNING) << "Rejected authorization for service: " << uuid
               << " requested from device: " << device_path.value();
  callback.Run(REJECTED);
}

void BluetoothDevice::ConfirmModeChange(Mode mode,
                                        const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  LOG(WARNING) << "Rejected adapter-level mode change: " << mode
               << " made on agent for device: " << address_;
  callback.Run(REJECTED);
}

void BluetoothDevice::Cancel() {
  DCHECK(agent_.get());
  DVLOG(1) << "Cancel: " << address_;

  DCHECK(pairing_delegate_);
  pairing_delegate_->DismissDisplayOrConfirm();
}


// static
BluetoothDevice* BluetoothDevice::Create(BluetoothAdapter* adapter) {
  return new BluetoothDevice(adapter);
}

}  // namespace chromeos

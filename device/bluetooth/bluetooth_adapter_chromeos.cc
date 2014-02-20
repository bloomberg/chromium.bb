// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_manager_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace {

// The agent path is relatively meaningless since BlueZ only permits one to
// exist per D-Bus connection, it just has to be unique within Chromium.
const char kAgentPath[] = "/org/chromium/bluetooth_agent";

// Histogram enumerations for pairing methods.
enum UMAPairingMethod {
  UMA_PAIRING_METHOD_NONE,
  UMA_PAIRING_METHOD_REQUEST_PINCODE,
  UMA_PAIRING_METHOD_REQUEST_PASSKEY,
  UMA_PAIRING_METHOD_DISPLAY_PINCODE,
  UMA_PAIRING_METHOD_DISPLAY_PASSKEY,
  UMA_PAIRING_METHOD_CONFIRM_PASSKEY,
  // NOTE: Add new pairing methods immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordingly.
  UMA_PAIRING_METHOD_COUNT
};

void OnUnregisterAgentError(const std::string& error_name,
                            const std::string& error_message) {
  LOG(WARNING) << "Failed to unregister pairing agent: "
               << error_name << ": " << error_message;
}

}  // namespace

namespace chromeos {

BluetoothAdapterChromeOS::BluetoothAdapterChromeOS()
    : weak_ptr_factory_(this) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothInputClient()->AddObserver(this);

  std::vector<dbus::ObjectPath> object_paths =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->GetAdapters();

  if (!object_paths.empty()) {
    VLOG(1) << object_paths.size() << " Bluetooth adapter(s) available.";
    SetAdapter(object_paths[0]);
  }

  // Register the pairing agent.
  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  agent_.reset(BluetoothAgentServiceProvider::Create(
      system_bus, dbus::ObjectPath(kAgentPath), this));
  DCHECK(agent_.get());

  VLOG(1) << "Registering pairing agent";
  DBusThreadManager::Get()->GetBluetoothAgentManagerClient()->
      RegisterAgent(
          dbus::ObjectPath(kAgentPath),
          bluetooth_agent_manager::kKeyboardDisplayCapability,
          base::Bind(&BluetoothAdapterChromeOS::OnRegisterAgent,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothAdapterChromeOS::OnRegisterAgentError,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothAdapterChromeOS::~BluetoothAdapterChromeOS() {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothInputClient()->RemoveObserver(this);

  VLOG(1) << "Unregistering pairing agent";
  DBusThreadManager::Get()->GetBluetoothAgentManagerClient()->
      UnregisterAgent(
          dbus::ObjectPath(kAgentPath),
          base::Bind(&base::DoNothing),
          base::Bind(&OnUnregisterAgentError));
}

void BluetoothAdapterChromeOS::AddObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterChromeOS::RemoveObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothAdapterChromeOS::GetAddress() const {
  if (!IsPresent())
    return std::string();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->address.value();
}

std::string BluetoothAdapterChromeOS::GetName() const {
  if (!IsPresent())
    return std::string();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

void BluetoothAdapterChromeOS::SetName(const std::string& name,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->alias.Set(
          name,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsInitialized() const {
  return true;
}

bool BluetoothAdapterChromeOS::IsPresent() const {
  return !object_path_.value().empty();
}

bool BluetoothAdapterChromeOS::IsPowered() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->powered.value();
}

void BluetoothAdapterChromeOS::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->powered.Set(
          powered,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsDiscoverable() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->discoverable.value();
}

void BluetoothAdapterChromeOS::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->discoverable.Set(
          discoverable,
          base::Bind(&BluetoothAdapterChromeOS::OnSetDiscoverable,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsDiscovering() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->discovering.value();
}

void BluetoothAdapterChromeOS::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // BlueZ counts discovery sessions, and permits multiple sessions for a
  // single connection, so issue a StartDiscovery() call for every use
  // within Chromium for the right behavior.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      StartDiscovery(
          object_path_,
          base::Bind(&BluetoothAdapterChromeOS::OnStartDiscovery,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          base::Bind(&BluetoothAdapterChromeOS::OnStartDiscoveryError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
}

void BluetoothAdapterChromeOS::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Inform BlueZ to stop one of our open discovery sessions.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      StopDiscovery(
          object_path_,
          base::Bind(&BluetoothAdapterChromeOS::OnStopDiscovery,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          base::Bind(&BluetoothAdapterChromeOS::OnStopDiscoveryError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
}

void BluetoothAdapterChromeOS::ReadLocalOutOfBandPairingData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterChromeOS::AdapterAdded(
    const dbus::ObjectPath& object_path) {
  // Set the adapter to the newly added adapter only if no adapter is present.
  if (!IsPresent())
    SetAdapter(object_path);
}

void BluetoothAdapterChromeOS::AdapterRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapterChromeOS::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  if (property_name == properties->powered.name())
    PoweredChanged(properties->powered.value());
  else if (property_name == properties->discoverable.name())
    DiscoverableChanged(properties->discoverable.value());
  else if (property_name == properties->discovering.name())
    DiscoveringChanged(properties->discovering.value());
}

void BluetoothAdapterChromeOS::DeviceAdded(
  const dbus::ObjectPath& object_path) {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path);
  if (properties->adapter.value() != object_path_)
    return;

  BluetoothDeviceChromeOS* device_chromeos =
      new BluetoothDeviceChromeOS(this, object_path);
  DCHECK(devices_.find(device_chromeos->GetAddress()) == devices_.end());

  devices_[device_chromeos->GetAddress()] = device_chromeos;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceAdded(this, device_chromeos));
}

void BluetoothAdapterChromeOS::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path) {
      devices_.erase(iter);

      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device_chromeos));
      delete device_chromeos;
      return;
    }
  }
}

void BluetoothAdapterChromeOS::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path);

  if (property_name == properties->bluetooth_class.name() ||
      property_name == properties->address.name() ||
      property_name == properties->alias.name() ||
      property_name == properties->paired.name() ||
      property_name == properties->trusted.name() ||
      property_name == properties->connected.name() ||
      property_name == properties->uuids.name())
    NotifyDeviceChanged(device_chromeos);

  // UMA connection counting
  if (property_name == properties->connected.name()) {
    int count = 0;

    for (DevicesMap::iterator iter = devices_.begin();
         iter != devices_.end(); ++iter) {
      if (iter->second->IsPaired() && iter->second->IsConnected())
        ++count;
    }

    UMA_HISTOGRAM_COUNTS_100("Bluetooth.ConnectedDeviceCount", count);
  }
}

void BluetoothAdapterChromeOS::InputPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  BluetoothInputClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothInputClient()->
          GetProperties(object_path);

  // Properties structure can be removed, which triggers a change in the
  // BluetoothDevice::IsConnectable() property, as does a change in the
  // actual reconnect_mode property.
  if (!properties ||
      property_name == properties->reconnect_mode.name())
    NotifyDeviceChanged(device_chromeos);
}

void BluetoothAdapterChromeOS::Release() {
  DCHECK(agent_.get());
  VLOG(1) << "Release";

  // Called after we unregister the pairing agent, e.g. when changing I/O
  // capabilities. Nothing much to be done right now.
}

void BluetoothAdapterChromeOS::RequestPinCode(
    const dbus::ObjectPath& device_path,
    const PinCodeCallback& callback) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestPinCode";

  BluetoothDeviceChromeOS* device_chromeos;
  PairingContext* pairing_context;
  if (!GetDeviceAndPairingContext(device_path,
                                  &device_chromeos, &pairing_context)) {
    callback.Run(REJECTED, "");
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_REQUEST_PINCODE,
                            UMA_PAIRING_METHOD_COUNT);

  DCHECK(pairing_context->pincode_callback_.is_null());
  pairing_context->pincode_callback_ = callback;
  pairing_context->pairing_delegate_->RequestPinCode(device_chromeos);
  pairing_context->pairing_delegate_used_ = true;
}

void BluetoothAdapterChromeOS::DisplayPinCode(
    const dbus::ObjectPath& device_path,
    const std::string& pincode) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": DisplayPinCode: " << pincode;

  BluetoothDeviceChromeOS* device_chromeos;
  PairingContext* pairing_context;
  if (!GetDeviceAndPairingContext(device_path,
                                  &device_chromeos, &pairing_context))
    return;

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_DISPLAY_PINCODE,
                            UMA_PAIRING_METHOD_COUNT);

  pairing_context->pairing_delegate_->DisplayPinCode(device_chromeos, pincode);
  pairing_context->pairing_delegate_used_ = true;
}

void BluetoothAdapterChromeOS::RequestPasskey(
    const dbus::ObjectPath& device_path,
    const PasskeyCallback& callback) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestPasskey";

  BluetoothDeviceChromeOS* device_chromeos;
  PairingContext* pairing_context;
  if (!GetDeviceAndPairingContext(device_path,
                                  &device_chromeos, &pairing_context)) {
    callback.Run(REJECTED, 0);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_REQUEST_PASSKEY,
                            UMA_PAIRING_METHOD_COUNT);

  DCHECK(pairing_context->passkey_callback_.is_null());
  pairing_context->passkey_callback_ = callback;
  pairing_context->pairing_delegate_->RequestPasskey(device_chromeos);
  pairing_context->pairing_delegate_used_ = true;
}

void BluetoothAdapterChromeOS::DisplayPasskey(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    uint16 entered) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": DisplayPasskey: " << passkey
          << " (" << entered << " entered)";

  BluetoothDeviceChromeOS* device_chromeos;
  PairingContext* pairing_context;
  if (!GetDeviceAndPairingContext(device_path,
                                  &device_chromeos, &pairing_context))
    return;

  if (entered == 0)
    UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                              UMA_PAIRING_METHOD_DISPLAY_PASSKEY,
                              UMA_PAIRING_METHOD_COUNT);

  if (entered == 0)
    pairing_context->pairing_delegate_->DisplayPasskey(device_chromeos,
                                                        passkey);

  pairing_context->pairing_delegate_->KeysEntered(device_chromeos, entered);
  pairing_context->pairing_delegate_used_ = true;
}

void BluetoothAdapterChromeOS::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestConfirmation: " << passkey;

  BluetoothDeviceChromeOS* device_chromeos;
  PairingContext* pairing_context;
  if (!GetDeviceAndPairingContext(device_path,
                                  &device_chromeos, &pairing_context)) {
    callback.Run(REJECTED);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_CONFIRM_PASSKEY,
                            UMA_PAIRING_METHOD_COUNT);

  DCHECK(pairing_context->confirmation_callback_.is_null());
  pairing_context->confirmation_callback_ = callback;
  pairing_context->pairing_delegate_->ConfirmPasskey(device_chromeos, passkey);
  pairing_context->pairing_delegate_used_ = true;
}

void BluetoothAdapterChromeOS::RequestAuthorization(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestAuthorization";

  // TODO(keybuk): implement
  callback.Run(CANCELLED);
}

void BluetoothAdapterChromeOS::AuthorizeService(
    const dbus::ObjectPath& device_path,
    const std::string& uuid,
    const ConfirmationCallback& callback) {
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": AuthorizeService: " << uuid;

  // TODO(keybuk): implement
  callback.Run(CANCELLED);
}

void BluetoothAdapterChromeOS::Cancel() {
  DCHECK(agent_.get());
  VLOG(1) << "Cancel";
}

bool BluetoothAdapterChromeOS::PairingContext::ExpectingPinCode() const {
  return !pincode_callback_.is_null();
}

bool BluetoothAdapterChromeOS::PairingContext::ExpectingPasskey() const {
  return !passkey_callback_.is_null();
}

bool BluetoothAdapterChromeOS::PairingContext::ExpectingConfirmation() const {
  return !confirmation_callback_.is_null();
}

void BluetoothAdapterChromeOS::PairingContext::SetPinCode(
    const std::string& pincode) {
  if (pincode_callback_.is_null())
    return;

  pincode_callback_.Run(SUCCESS, pincode);
  pincode_callback_.Reset();
}

void BluetoothAdapterChromeOS::PairingContext::SetPasskey(uint32 passkey) {
  if (passkey_callback_.is_null())
    return;

  passkey_callback_.Run(SUCCESS, passkey);
  passkey_callback_.Reset();
}

void BluetoothAdapterChromeOS::PairingContext::ConfirmPairing() {
  if (confirmation_callback_.is_null())
    return;

  confirmation_callback_.Run(SUCCESS);
  confirmation_callback_.Reset();
}

bool BluetoothAdapterChromeOS::PairingContext::RejectPairing() {
  return RunPairingCallbacks(REJECTED);
}

bool BluetoothAdapterChromeOS::PairingContext::CancelPairing() {
  return RunPairingCallbacks(CANCELLED);
}

BluetoothAdapterChromeOS::PairingContext::PairingContext(
    BluetoothDevice::PairingDelegate* pairing_delegate)
    : pairing_delegate_(pairing_delegate),
      pairing_delegate_used_(false) {
  VLOG(1) << "Created PairingContext";
}

BluetoothAdapterChromeOS::PairingContext::~PairingContext() {
  VLOG(1) << "Destroying PairingContext";

  if (!pairing_delegate_used_)
    UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                              UMA_PAIRING_METHOD_NONE,
                              UMA_PAIRING_METHOD_COUNT);

  DCHECK(pincode_callback_.is_null());
  DCHECK(passkey_callback_.is_null());
  DCHECK(confirmation_callback_.is_null());

  pairing_delegate_->DismissDisplayOrConfirm();
  pairing_delegate_ = NULL;
}

bool BluetoothAdapterChromeOS::PairingContext::RunPairingCallbacks(
    BluetoothAgentServiceProvider::Delegate::Status status) {
  pairing_delegate_used_ = true;

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

void BluetoothAdapterChromeOS::OnRegisterAgent() {
  VLOG(1) << "Pairing agent registered";
}

void BluetoothAdapterChromeOS::OnRegisterAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << ": Failed to register pairing agent: "
               << error_name << ": " << error_message;

  agent_.reset();
}

BluetoothDeviceChromeOS*
BluetoothAdapterChromeOS::GetDeviceWithPath(
    const dbus::ObjectPath& object_path) {
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path)
      return device_chromeos;
  }

  return NULL;
}

bool BluetoothAdapterChromeOS::GetDeviceAndPairingContext(
    const dbus::ObjectPath& object_path,
    BluetoothDeviceChromeOS** device_chromeos,
    PairingContext** pairing_context)
{
  *device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos) {
    LOG(WARNING) << "Pairing Agent request for unknown device: "
                 << object_path.value();
    return false;
  }

  *pairing_context = (*device_chromeos)->pairing_context_.get();
  if (*pairing_context)
    return true;

  // TODO(keybuk): this is the point we need a default pairing delegate, create
  // a PairingContext with that passed in, set it as the context on the device
  // and return true.
  return false;
}

void BluetoothAdapterChromeOS::SetAdapter(const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  object_path_ = object_path;

  VLOG(1) << object_path_.value() << ": using adapter.";

  SetDefaultAdapterName();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  PresentChanged(true);

  if (properties->powered.value())
    PoweredChanged(true);
  if (properties->discoverable.value())
    DiscoverableChanged(true);
  if (properties->discovering.value())
    DiscoveringChanged(true);

  std::vector<dbus::ObjectPath> device_paths =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetDevicesForAdapter(object_path_);

  for (std::vector<dbus::ObjectPath>::iterator iter = device_paths.begin();
       iter != device_paths.end(); ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        new BluetoothDeviceChromeOS(this, *iter);

    devices_[device_chromeos->GetAddress()] = device_chromeos;

    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device_chromeos));
  }
}

void BluetoothAdapterChromeOS::SetDefaultAdapterName() {
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  std::string alias;
  if (board.substr(0, 6) == "stumpy") {
    alias = "Chromebox";
  } else if (board.substr(0, 4) == "link") {
    alias = "Chromebook Pixel";
  } else {
    alias = "Chromebook";
  }

  SetName(alias, base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
}

void BluetoothAdapterChromeOS::RemoveAdapter() {
  DCHECK(IsPresent());
  VLOG(1) << object_path_.value() << ": adapter removed.";

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  object_path_ = dbus::ObjectPath("");

  if (properties->powered.value())
    PoweredChanged(false);
  if (properties->discoverable.value())
    DiscoverableChanged(false);
  if (properties->discovering.value())
    DiscoveringChanged(false);

  // Copy the devices list here and clear the original so that when we
  // send DeviceRemoved(), GetDevices() returns no devices.
  DevicesMap devices = devices_;
  devices_.clear();

  for (DevicesMap::iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, iter->second));
    delete iter->second;
  }

  PresentChanged(false);
}

void BluetoothAdapterChromeOS::PoweredChanged(bool powered) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered));
}

void BluetoothAdapterChromeOS::DiscoverableChanged(bool discoverable) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoverableChanged(this, discoverable));
}

void BluetoothAdapterChromeOS::DiscoveringChanged(
    bool discovering) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, discovering));
}

void BluetoothAdapterChromeOS::PresentChanged(bool present) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, present));
}

void BluetoothAdapterChromeOS::NotifyDeviceChanged(
    BluetoothDeviceChromeOS* device) {
  DCHECK(device->adapter_ == this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceChanged(this, device));
}

void BluetoothAdapterChromeOS::OnSetDiscoverable(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  // Set the discoverable_timeout property to zero so the adapter remains
  // discoverable forever.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->discoverable_timeout.Set(
          0,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void BluetoothAdapterChromeOS::OnPropertyChangeCompleted(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothAdapterChromeOS::OnStartDiscovery(const base::Closure& callback) {
  callback.Run();
}

void BluetoothAdapterChromeOS::OnStartDiscoveryError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to start discovery: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothAdapterChromeOS::OnStopDiscovery(const base::Closure& callback) {
  callback.Run();
}

void BluetoothAdapterChromeOS::OnStopDiscoveryError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to stop discovery: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

}  // namespace chromeos

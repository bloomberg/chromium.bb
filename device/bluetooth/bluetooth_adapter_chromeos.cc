// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

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
}

BluetoothAdapterChromeOS::~BluetoothAdapterChromeOS() {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothInputClient()->RemoveObserver(this);
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
          base::Bind(&BluetoothAdapterChromeOS::OnSetPowered,
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

void BluetoothAdapterChromeOS::SetAdapter(const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  object_path_ = object_path;

  VLOG(1) << object_path_.value() << ": using adapter.";

  SetAdapterName();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  PresentChanged(true);

  if (properties->powered.value())
    PoweredChanged(true);
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

void BluetoothAdapterChromeOS::SetAdapterName() {
  // Set a better name for the adapter than "BlueZ 5.x"; this isn't an ideal
  // way to do this but it'll do for now. See http://crbug.com/126732 and
  // http://crbug.com/126802.
  std::string board;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kChromeOSReleaseBoard)) {
    board = command_line->
        GetSwitchValueASCII(chromeos::switches::kChromeOSReleaseBoard);
  }

  std::string alias;
  if (board.substr(0, 6) == "stumpy") {
    alias = "Chromebox";
  } else if (board.substr(0, 4) == "link") {
    alias = "Chromebook Pixel";
  } else {
    alias = "Chromebook";
  }

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->alias.Set(
          alias,
          base::Bind(&BluetoothAdapterChromeOS::OnSetAlias,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterChromeOS::OnSetAlias(bool success) {
  LOG_IF(WARNING, !success) << object_path_.value()
                            << ": Failed to set adapter alias";
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

void BluetoothAdapterChromeOS::OnSetPowered(const base::Closure& callback,
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

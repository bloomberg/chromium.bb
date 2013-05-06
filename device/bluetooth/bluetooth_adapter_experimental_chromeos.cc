// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_experimental_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/experimental_bluetooth_adapter_client.h"
#include "chromeos/dbus/experimental_bluetooth_device_client.h"
#include "chromeos/dbus/experimental_bluetooth_input_client.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_experimental_chromeos.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace chromeos {

BluetoothAdapterExperimentalChromeOS::BluetoothAdapterExperimentalChromeOS()
    : weak_ptr_factory_(this) {
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetExperimentalBluetoothInputClient()->
      AddObserver(this);

  std::vector<dbus::ObjectPath> object_paths =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetAdapters();

  if (!object_paths.empty()) {
    VLOG(1) << object_paths.size() << " Bluetooth adapter(s) available.";
    SetAdapter(object_paths[0]);
  }
}

BluetoothAdapterExperimentalChromeOS::~BluetoothAdapterExperimentalChromeOS() {
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetExperimentalBluetoothInputClient()->
      RemoveObserver(this);
}

void BluetoothAdapterExperimentalChromeOS::AddObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterExperimentalChromeOS::RemoveObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothAdapterExperimentalChromeOS::GetAddress() const {
  if (!IsPresent())
    return std::string();

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->address.value();
}

std::string BluetoothAdapterExperimentalChromeOS::GetName() const {
  if (!IsPresent())
    return std::string();

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

bool BluetoothAdapterExperimentalChromeOS::IsInitialized() const {
  return true;
}

bool BluetoothAdapterExperimentalChromeOS::IsPresent() const {
  return !object_path_.value().empty();
}

bool BluetoothAdapterExperimentalChromeOS::IsPowered() const {
  if (!IsPresent())
    return false;

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->powered.value();
}

void BluetoothAdapterExperimentalChromeOS::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      GetProperties(object_path_)->powered.Set(
          powered,
          base::Bind(&BluetoothAdapterExperimentalChromeOS::OnSetPowered,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterExperimentalChromeOS::IsDiscovering() const {
  if (!IsPresent())
    return false;

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->discovering.value();
}

void BluetoothAdapterExperimentalChromeOS::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // BlueZ counts discovery sessions, and permits multiple sessions for a
  // single connection, so issue a StartDiscovery() call for every use
  // within Chromium for the right behavior.
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      StartDiscovery(
          object_path_,
          base::Bind(
              &BluetoothAdapterExperimentalChromeOS::OnStartDiscovery,
              weak_ptr_factory_.GetWeakPtr(),
              callback),
          base::Bind(
              &BluetoothAdapterExperimentalChromeOS::OnStartDiscoveryError,
              weak_ptr_factory_.GetWeakPtr(),
              error_callback));
}

void BluetoothAdapterExperimentalChromeOS::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Inform BlueZ to stop one of our open discovery sessions.
  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      StopDiscovery(
          object_path_,
          base::Bind(
              &BluetoothAdapterExperimentalChromeOS::OnStopDiscovery,
              weak_ptr_factory_.GetWeakPtr(),
              callback),
          base::Bind(
              &BluetoothAdapterExperimentalChromeOS::OnStopDiscoveryError,
              weak_ptr_factory_.GetWeakPtr(),
              error_callback));
}

void BluetoothAdapterExperimentalChromeOS::ReadLocalOutOfBandPairingData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::AdapterAdded(
    const dbus::ObjectPath& object_path) {
  // Set the adapter to the newly added adapter only if no adapter is present.
  if (!IsPresent())
    SetAdapter(object_path);
}

void BluetoothAdapterExperimentalChromeOS::AdapterRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapterExperimentalChromeOS::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);

  if (property_name == properties->powered.name())
    PoweredChanged(properties->powered.value());
  else if (property_name == properties->discovering.name())
    DiscoveringChanged(properties->discovering.value());
}

void BluetoothAdapterExperimentalChromeOS::DeviceAdded(
  const dbus::ObjectPath& object_path) {
  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetProperties(object_path);
  if (properties->adapter.value() != object_path_)
    return;

  BluetoothDeviceExperimentalChromeOS* device_chromeos =
      new BluetoothDeviceExperimentalChromeOS(this, object_path);
  DCHECK(devices_.find(device_chromeos->GetAddress()) == devices_.end());

  devices_[device_chromeos->GetAddress()] = device_chromeos;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceAdded(this, device_chromeos));
}

void BluetoothAdapterExperimentalChromeOS::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceExperimentalChromeOS* device_chromeos =
        static_cast<BluetoothDeviceExperimentalChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path) {
      devices_.erase(iter);

      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device_chromeos));
      delete device_chromeos;
      return;
    }
  }
}

void BluetoothAdapterExperimentalChromeOS::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceExperimentalChromeOS* device_chromeos =
      GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  ExperimentalBluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
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

void BluetoothAdapterExperimentalChromeOS::InputPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceExperimentalChromeOS* device_chromeos =
      GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  ExperimentalBluetoothInputClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothInputClient()->
          GetProperties(object_path);

  // Properties structure can be removed, which triggers a change in the
  // BluetoothDevice::IsConnectable() property, as does a change in the
  // actual reconnect_mode property.
  if (!properties ||
      property_name == properties->reconnect_mode.name())
    NotifyDeviceChanged(device_chromeos);
}

BluetoothDeviceExperimentalChromeOS*
BluetoothAdapterExperimentalChromeOS::GetDeviceWithPath(
    const dbus::ObjectPath& object_path) {
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceExperimentalChromeOS* device_chromeos =
        static_cast<BluetoothDeviceExperimentalChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path)
      return device_chromeos;
  }

  return NULL;
}

void BluetoothAdapterExperimentalChromeOS::SetAdapter(
    const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  object_path_ = object_path;

  VLOG(1) << object_path_.value() << ": using adapter.";

  SetAdapterName();

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
          GetProperties(object_path_);

  PresentChanged(true);

  if (properties->powered.value())
    PoweredChanged(true);
  if (properties->discovering.value())
    DiscoveringChanged(true);

  std::vector<dbus::ObjectPath> device_paths =
      DBusThreadManager::Get()->GetExperimentalBluetoothDeviceClient()->
          GetDevicesForAdapter(object_path_);

  for (std::vector<dbus::ObjectPath>::iterator iter = device_paths.begin();
       iter != device_paths.end(); ++iter) {
    BluetoothDeviceExperimentalChromeOS* device_chromeos =
        new BluetoothDeviceExperimentalChromeOS(this, *iter);

    devices_[device_chromeos->GetAddress()] = device_chromeos;

    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device_chromeos));
  }
}

void BluetoothAdapterExperimentalChromeOS::SetAdapterName() {
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

  DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
      GetProperties(object_path_)->alias.Set(
          alias,
          base::Bind(&BluetoothAdapterExperimentalChromeOS::OnSetAlias,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterExperimentalChromeOS::OnSetAlias(bool success) {
  LOG_IF(WARNING, !success) << object_path_.value()
                            << ": Failed to set adapter alias";
}

void BluetoothAdapterExperimentalChromeOS::RemoveAdapter() {
  DCHECK(IsPresent());
  VLOG(1) << object_path_.value() << ": adapter removed.";

  ExperimentalBluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetExperimentalBluetoothAdapterClient()->
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

void BluetoothAdapterExperimentalChromeOS::PoweredChanged(bool powered) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered));
}

void BluetoothAdapterExperimentalChromeOS::DiscoveringChanged(
    bool discovering) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, discovering));
}

void BluetoothAdapterExperimentalChromeOS::PresentChanged(bool present) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, present));
}

void BluetoothAdapterExperimentalChromeOS::NotifyDeviceChanged(
    BluetoothDeviceExperimentalChromeOS* device) {
  DCHECK(device->adapter_ == this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceChanged(this, device));
}

void BluetoothAdapterExperimentalChromeOS::OnSetPowered(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::OnStartDiscovery(
    const base::Closure& callback) {
  callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::OnStartDiscoveryError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to start discovery: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::OnStopDiscovery(
    const base::Closure& callback) {
  callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::OnStopDiscoveryError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to stop discovery: "
               << error_name << ": " << error_message;
  error_callback.Run();
}

}  // namespace chromeos

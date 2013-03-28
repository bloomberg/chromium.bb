// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothOutOfBandPairingData;

namespace chromeos {

BluetoothAdapterChromeOS::BluetoothAdapterChromeOS() : BluetoothAdapter(),
                                                       track_default_(false),
                                                       powered_(false),
                                                       discovering_(false),
                                                       discovering_count_(0),
                                                       weak_ptr_factory_(this) {
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      AddObserver(this);
}

BluetoothAdapterChromeOS::~BluetoothAdapterChromeOS() {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      RemoveObserver(this);

  STLDeleteValues(&devices_);
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

// TODO(youngki) Return true when object path and properties of the adapter are
// initialized.
bool BluetoothAdapterChromeOS::IsInitialized() const {
  return true;
}

bool BluetoothAdapterChromeOS::IsPresent() const {
  return !object_path_.value().empty() && !address_.empty();
}

bool BluetoothAdapterChromeOS::IsPowered() const {
  return powered_;
}

void BluetoothAdapterChromeOS::SetPowered(bool powered,
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
  return discovering_;
}

void BluetoothAdapterChromeOS::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // BlueZ counts discovery sessions, and permits multiple sessions for a
  // single connection, so issue a StartDiscovery() call for every use
  // within Chromium for the right behavior.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      StartDiscovery(object_path_,
                     base::Bind(
                         &BluetoothAdapterChromeOS::OnStartDiscovery,
                         weak_ptr_factory_.GetWeakPtr(),
                         callback, error_callback));
}

void BluetoothAdapterChromeOS::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Inform BlueZ to stop one of our open discovery sessions.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      StopDiscovery(object_path_,
                    base::Bind(
                        &BluetoothAdapterChromeOS::OnStopDiscovery,
                        weak_ptr_factory_.GetWeakPtr(),
                        callback, error_callback));
}

void BluetoothAdapterChromeOS::ReadLocalOutOfBandPairingData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      ReadLocalData(object_path_,
          base::Bind(&BluetoothAdapterChromeOS::OnReadLocalData,
              weak_ptr_factory_.GetWeakPtr(),
              callback,
              error_callback));
}

void BluetoothAdapterChromeOS::TrackDefaultAdapter() {
  VLOG(1) << "Tracking default adapter";
  track_default_ = true;
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      DefaultAdapter(base::Bind(&BluetoothAdapterChromeOS::AdapterCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterChromeOS::AdapterCallback(
    const dbus::ObjectPath& adapter_path,
    bool success) {
  if (success) {
    ChangeAdapter(adapter_path);
  } else if (!object_path_.value().empty()) {
    RemoveAdapter();
  }
}

void BluetoothAdapterChromeOS::DefaultAdapterChanged(
    const dbus::ObjectPath& adapter_path) {
  if (track_default_)
    ChangeAdapter(adapter_path);
}

void BluetoothAdapterChromeOS::AdapterRemoved(
    const dbus::ObjectPath& adapter_path) {
  if (adapter_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapterChromeOS::ChangeAdapter(
    const dbus::ObjectPath& adapter_path) {
  if (object_path_.value().empty()) {
    VLOG(1) << "Adapter path initialized to " << adapter_path.value();
  } else if (object_path_.value() != adapter_path.value()) {
    VLOG(1) << "Adapter path changed from " << object_path_.value()
            << " to " << adapter_path.value();

    RemoveAdapter();
  } else {
    VLOG(1) << "Adapter address updated";
  }

  object_path_ = adapter_path;

  // Update properties to their new values.
  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_);

  address_ = properties->address.value();
  name_ = properties->name.value();

  // Delay announcing a new adapter until we have an address.
  if (address_.empty()) {
    VLOG(1) << "Adapter address not yet known";
    return;
  }

  PoweredChanged(properties->powered.value());
  DiscoveringChanged(properties->discovering.value());
  DevicesChanged(properties->devices.value());

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, true));
}

void BluetoothAdapterChromeOS::RemoveAdapter() {
  const bool adapter_was_present = IsPresent();

  VLOG(1) << "Adapter lost.";
  PoweredChanged(false);
  DiscoveringChanged(false);
  ClearDevices();

  object_path_ = dbus::ObjectPath("");
  address_.clear();
  name_.clear();

  if (adapter_was_present)
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, false));
}

void BluetoothAdapterChromeOS::OnSetPowered(const base::Closure& callback,
                                            const ErrorCallback& error_callback,
                                            bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothAdapterChromeOS::PoweredChanged(bool powered) {
  if (powered == powered_)
    return;

  powered_ = powered;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered_));
}

void BluetoothAdapterChromeOS::OnStartDiscovery(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    const dbus::ObjectPath& adapter_path,
    bool success) {
  if (success) {
    if (discovering_count_++ == 0) {
      VLOG(1) << object_path_.value() << ": started discovery.";

      // Clear devices found in previous discovery attempts
      ClearDiscoveredDevices();
    }

    callback.Run();
  } else {
    error_callback.Run();
  }
}

void BluetoothAdapterChromeOS::OnStopDiscovery(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    const dbus::ObjectPath& adapter_path,
    bool success) {
  if (success) {
    if (--discovering_count_ == 0) {
      VLOG(1) << object_path_.value() << ": stopped discovery.";
    } else if (discovering_count_ < 0) {
      LOG(WARNING) << adapter_path.value() << ": call to StopDiscovering "
                   << "without matching StartDiscovering.";
      error_callback.Run();
      return;
    }

    callback.Run();

    // Leave found devices available for perusing.
  } else {
    error_callback.Run();
  }
}

void BluetoothAdapterChromeOS::DiscoveringChanged(bool discovering) {
  if (discovering == discovering_)
    return;

  discovering_ = discovering;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, discovering_));
}

void BluetoothAdapterChromeOS::OnReadLocalData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback,
    const BluetoothOutOfBandPairingData& data,
    bool success) {
  if (success)
    callback.Run(data);
  else
    error_callback.Run();
}

void BluetoothAdapterChromeOS::AdapterPropertyChanged(
    const dbus::ObjectPath& adapter_path,
    const std::string& property_name) {
  if (adapter_path != object_path_)
    return;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_);

  if (property_name == properties->address.name()) {
    ChangeAdapter(object_path_);

  } else if (!address_.empty()) {
    if (property_name == properties->powered.name()) {
      PoweredChanged(properties->powered.value());

    } else if (property_name == properties->discovering.name()) {
      DiscoveringChanged(properties->discovering.value());

    } else if (property_name == properties->devices.name()) {
      DevicesChanged(properties->devices.value());

    } else if (property_name == properties->name.name()) {
      name_ = properties->name.value();

    }
  }
}

void BluetoothAdapterChromeOS::DevicePropertyChanged(
    const dbus::ObjectPath& device_path,
    const std::string& property_name) {
  UpdateDevice(device_path);
}

void BluetoothAdapterChromeOS::UpdateDevice(
    const dbus::ObjectPath& device_path) {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      GetProperties(device_path);

  // When we first see a device, we may not know the address yet and need to
  // wait for the DevicePropertyChanged signal before adding the device.
  const std::string address = properties->address.value();
  if (address.empty())
    return;

  // The device may be already known to us, either because this is an update
  // to properties, or the device going from discovered to connected and
  // pairing gaining an object path in the process. In any case, we want
  // to update the existing object, not create a new one.
  DevicesMap::iterator iter = devices_.find(address);
  BluetoothDeviceChromeOS* device;
  const bool update_device = (iter != devices_.end());
  if (update_device) {
    device = static_cast<BluetoothDeviceChromeOS*>(iter->second);
  } else {
    device = BluetoothDeviceChromeOS::Create(this);
    devices_[address] = device;
  }

  const bool was_paired = device->IsPaired();
  if (!was_paired) {
    VLOG(1) << "Assigned object path " << device_path.value() << " to device "
            << address;
    device->SetObjectPath(device_path);
  }
  device->Update(properties, true);

  if (update_device) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  } else {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device));
  }
}

void BluetoothAdapterChromeOS::ClearDevices() {
  DevicesMap replace;
  devices_.swap(replace);
  for (DevicesMap::iterator iter = replace.begin();
       iter != replace.end(); ++iter) {
    BluetoothDevice* device = iter->second;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, device));

    delete device;
  }
}

void BluetoothAdapterChromeOS::DeviceCreated(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  if (adapter_path != object_path_)
    return;

  UpdateDevice(device_path);
}

void BluetoothAdapterChromeOS::DeviceRemoved(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  if (adapter_path != object_path_)
    return;

  DevicesMap::iterator iter = devices_.begin();
  while (iter != devices_.end()) {
    BluetoothDeviceChromeOS* device =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);
    DevicesMap::iterator temp = iter;
    ++iter;

    if (device->object_path_ != device_path)
      continue;

    // DeviceRemoved can also be called to indicate a device that is visible
    // during discovery has disconnected, but it is still visible to the
    // adapter, so don't remove in that case and only clear the object path.
    if (!device->IsVisible()) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device));

      VLOG(1) << "Removed device " << device->address();

      delete device;
      devices_.erase(temp);
    } else {
      VLOG(1) << "Removed object path from device " << device->address();
      device->RemoveObjectPath();

      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceChanged(this, device));
    }
  }
}

void BluetoothAdapterChromeOS::DevicesChanged(
    const std::vector<dbus::ObjectPath>& devices) {
  for (std::vector<dbus::ObjectPath>::const_iterator iter =
           devices.begin(); iter != devices.end(); ++iter)
    UpdateDevice(*iter);
}

void BluetoothAdapterChromeOS::ClearDiscoveredDevices() {
  DevicesMap::iterator iter = devices_.begin();
  while (iter != devices_.end()) {
    BluetoothDevice* device = iter->second;
    DevicesMap::iterator temp = iter;
    ++iter;

    if (!device->IsPaired()) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device));

      delete device;
      devices_.erase(temp);
    }
  }
}

void BluetoothAdapterChromeOS::DeviceFound(
    const dbus::ObjectPath& adapter_path,
    const std::string& address,
    const BluetoothDeviceClient::Properties& properties) {
  if (adapter_path != object_path_)
    return;

  // DeviceFound can also be called to indicate that a device we've
  // paired with is now visible to the adapter during discovery, in which
  // case we want to update the existing object, not create a new one.
  BluetoothDeviceChromeOS* device;
  DevicesMap::iterator iter = devices_.find(address);
  const bool update_device = (iter != devices_.end());
  if (update_device) {
    device = static_cast<BluetoothDeviceChromeOS*>(iter->second);
  } else {
    device = BluetoothDeviceChromeOS::Create(this);
    devices_[address] = device;
  }

  VLOG(1) << "Device " << address << " is visible to the adapter";
  device->SetVisible(true);
  device->Update(&properties, false);

  if (update_device) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  } else {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device));
  }
}

void BluetoothAdapterChromeOS::DeviceDisappeared(
    const dbus::ObjectPath& adapter_path,
    const std::string& address) {
  if (adapter_path != object_path_)
    return;

  DevicesMap::iterator iter = devices_.find(address);
  if (iter == devices_.end())
    return;

  BluetoothDeviceChromeOS* device =
      static_cast<BluetoothDeviceChromeOS*>(iter->second);

  // DeviceDisappeared can also be called to indicate that a device we've
  // paired with is no longer visible to the adapter, so don't remove
  // in that case and only clear the visible flag.
  if (!device->IsPaired()) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, device));

    VLOG(1) << "Discovered device " << device->address()
             << " is no longer visible to the adapter";

    delete device;
    devices_.erase(iter);
  } else {
    VLOG(1) << "Paired device " << device->address()
            << " is no longer visible to the adapter";
    device->SetVisible(false);

    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  }
}

}  // namespace chromeos

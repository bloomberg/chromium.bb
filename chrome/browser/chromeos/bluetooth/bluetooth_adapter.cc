// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/object_path.h"

namespace chromeos {

BluetoothAdapter::BluetoothAdapter() : weak_ptr_factory_(this),
                                       track_default_(false),
                                       powered_(false),
                                       discovering_(false) {
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      AddObserver(this);
}

BluetoothAdapter::~BluetoothAdapter() {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      RemoveObserver(this);

  STLDeleteValues(&devices_);
}

void BluetoothAdapter::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapter::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void BluetoothAdapter::DefaultAdapter() {
  DVLOG(1) << "Tracking default adapter";
  track_default_ = true;
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      DefaultAdapter(base::Bind(&BluetoothAdapter::AdapterCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapter::FindAdapter(const std::string& address) {
  DVLOG(1) << "Using adapter " << address;
  track_default_ = false;
  DBusThreadManager::Get()->GetBluetoothManagerClient()->
      FindAdapter(address,
                  base::Bind(&BluetoothAdapter::AdapterCallback,
                             weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapter::AdapterCallback(const dbus::ObjectPath& adapter_path,
                                       bool success) {
  if (success) {
    ChangeAdapter(adapter_path);
  } else if (!object_path_.value().empty()) {
    RemoveAdapter();
  }
}

void BluetoothAdapter::DefaultAdapterChanged(
    const dbus::ObjectPath& adapter_path) {
  if (track_default_)
    ChangeAdapter(adapter_path);
}

void BluetoothAdapter::AdapterRemoved(const dbus::ObjectPath& adapter_path) {
  if (adapter_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapter::ChangeAdapter(const dbus::ObjectPath& adapter_path) {
  if (adapter_path == object_path_)
    return;

  // Determine whether this is a change of adapter or gaining an adapter,
  // remember for later so we can send the right notification.
  bool was_present = false;
  if (object_path_.value().empty()) {
    DVLOG(1) << "Adapter path initialized to " << adapter_path.value();
    was_present = false;
  } else {
    DVLOG(1) << "Adapter path changed from " << object_path_.value()
             << " to " << adapter_path.value();
    was_present = true;

    // Invalidate the devices list, since the property update does not
    // remove them.
    ClearDevices();
  }

  object_path_ = adapter_path;

  // Update properties to their new values.
  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_);

  address_ = properties->address.value();

  PoweredChanged(properties->powered.value());
  DiscoveringChanged(properties->discovering.value());
  DevicesChanged(properties->devices.value());

  // Notify observers if we did not have an adapter before, the case of
  // moving from one to another is hidden from layers above.
  if (!was_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, true));
  }
}

void BluetoothAdapter::RemoveAdapter() {
  DVLOG(1) << "Adapter lost.";
  PoweredChanged(false);
  DiscoveringChanged(false);
  ClearDevices();

  object_path_ = dbus::ObjectPath("");
  address_.clear();

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, false));
}

bool BluetoothAdapter::IsPresent() const {
  return !object_path_.value().empty();
}

bool BluetoothAdapter::IsPowered() const {
  return powered_;
}

void BluetoothAdapter::SetPowered(bool powered,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->powered.Set(
          powered,
          base::Bind(&BluetoothAdapter::OnSetPowered,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void BluetoothAdapter::OnSetPowered(const base::Closure& callback,
                                    const ErrorCallback& error_callback,
                                    bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run();
}

void BluetoothAdapter::PoweredChanged(bool powered) {
  if (powered == powered_)
    return;

  powered_ = powered;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered_));
}

bool BluetoothAdapter::IsDiscovering() const {
  return discovering_;
}

void BluetoothAdapter::SetDiscovering(bool discovering,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  if (discovering) {
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->
        StartDiscovery(object_path_,
                       base::Bind(&BluetoothAdapter::OnStartDiscovery,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback,
                                  error_callback));
  } else {
    DBusThreadManager::Get()->GetBluetoothAdapterClient()->
        StopDiscovery(object_path_,
                      base::Bind(&BluetoothAdapter::OnStopDiscovery,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 callback,
                                 error_callback));
  }
}

void BluetoothAdapter::OnStartDiscovery(const base::Closure& callback,
                                        const ErrorCallback& error_callback,
                                        const dbus::ObjectPath& adapter_path,
                                        bool success) {
  if (success) {
    DVLOG(1) << object_path_.value() << ": started discovery.";

    // Clear devices found in previous discovery attempts
    ClearDiscoveredDevices();
    callback.Run();
  } else {
    // TODO(keybuk): in future, don't run the callback if the error was just
    // that we were already discovering.
    error_callback.Run();
  }
}

void BluetoothAdapter::OnStopDiscovery(const base::Closure& callback,
                                       const ErrorCallback& error_callback,
                                       const dbus::ObjectPath& adapter_path,
                                       bool success) {
  if (success) {
    DVLOG(1) << object_path_.value() << ": stopped discovery.";
    callback.Run();
    // Leave found devices available for perusing.
  } else {
    // TODO(keybuk): in future, don't run the callback if the error was just
    // that we weren't discovering.
    error_callback.Run();
  }
}

void BluetoothAdapter::DiscoveringChanged(bool discovering) {
  if (discovering == discovering_)
    return;

  discovering_ = discovering;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, discovering_));
}

void BluetoothAdapter::OnReadLocalData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback,
    const BluetoothOutOfBandPairingData& data,
    bool success) {
  if (success)
    callback.Run(data);
  else
    error_callback.Run();
}

void BluetoothAdapter::AdapterPropertyChanged(
    const dbus::ObjectPath& adapter_path,
    const std::string& property_name) {
  if (adapter_path != object_path_)
    return;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_);

  if (property_name == properties->powered.name()) {
    PoweredChanged(properties->powered.value());

  } else if (property_name == properties->discovering.name()) {
    DiscoveringChanged(properties->discovering.value());

  } else if (property_name == properties->devices.name()) {
    DevicesChanged(properties->devices.value());

  }
}

void BluetoothAdapter::DevicePropertyChanged(
    const dbus::ObjectPath& device_path,
    const std::string& property_name) {
  UpdateDevice(device_path);
}

void BluetoothAdapter::UpdateDevice(const dbus::ObjectPath& device_path) {
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
      GetProperties(device_path);

  // When we first see a device, we may not know the address yet and need to
  // wait for the DevicePropertyChanged signal before adding the device.
  const std::string address = properties->address.value();
  if (address.empty())
    return;

  // If the device is already known this may be just an update to properties,
  // or it may be the device going from discovered to connected and gaining
  // an object path. Update the existing object and notify observers.
  DevicesMap::iterator iter = devices_.find(address);
  if (iter != devices_.end()) {
    BluetoothDevice* device = iter->second;

    if (!device->IsPaired())
      device->SetObjectPath(device_path);
    device->Update(properties, true);

    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
    return;
  }

  // Device has an address and was not previously known, add to the map
  // and notify observers.
  BluetoothDevice* device = BluetoothDevice::CreateBound(this, device_path,
                                                         properties);
  devices_[address] = device;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceAdded(this, device));
}

BluetoothAdapter::DeviceList BluetoothAdapter::GetDevices() {
  ConstDeviceList const_devices =
      const_cast<const BluetoothAdapter *>(this)->GetDevices();

  DeviceList devices;
  for (ConstDeviceList::const_iterator i = const_devices.begin();
      i != const_devices.end(); ++i)
    devices.push_back(const_cast<BluetoothDevice *>(*i));

  return devices;
}

BluetoothAdapter::ConstDeviceList BluetoothAdapter::GetDevices() const {
  ConstDeviceList devices;
  for (DevicesMap::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter)
    devices.push_back(iter->second);

  return devices;
}

BluetoothDevice* BluetoothAdapter::GetDevice(const std::string& address) {
  return const_cast<BluetoothDevice *>(
      const_cast<const BluetoothAdapter *>(this)->GetDevice(address));
}

const BluetoothDevice* BluetoothAdapter::GetDevice(
    const std::string& address) const {
  DevicesMap::const_iterator iter = devices_.find(address);
  if (iter != devices_.end())
    return iter->second;

  return NULL;
}

void BluetoothAdapter::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetBluetoothOutOfBandClient()->
      ReadLocalData(object_path_,
          base::Bind(&BluetoothAdapter::OnReadLocalData,
              weak_ptr_factory_.GetWeakPtr(),
              callback,
              error_callback));
}

void BluetoothAdapter::ClearDevices() {
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

void BluetoothAdapter::DeviceCreated(const dbus::ObjectPath& adapter_path,
                                     const dbus::ObjectPath& device_path) {
  if (adapter_path != object_path_)
    return;

  UpdateDevice(device_path);
}

void BluetoothAdapter::DeviceRemoved(const dbus::ObjectPath& adapter_path,
                                     const dbus::ObjectPath& device_path) {
  if (adapter_path != object_path_)
    return;

  DevicesMap::iterator iter = devices_.begin();
  while (iter != devices_.end()) {
    BluetoothDevice* device = iter->second;
    DevicesMap::iterator temp = iter;
    ++iter;

    if (device->object_path_ == device_path) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device));

      delete device;
      devices_.erase(temp);
    }
  }
}

void BluetoothAdapter::DevicesChanged(
    const std::vector<dbus::ObjectPath>& devices) {
  for (std::vector<dbus::ObjectPath>::const_iterator iter =
           devices.begin(); iter != devices.end(); ++iter)
    UpdateDevice(*iter);
}

void BluetoothAdapter::ClearDiscoveredDevices() {
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

void BluetoothAdapter::DeviceFound(
    const dbus::ObjectPath& adapter_path, const std::string& address,
    const BluetoothDeviceClient::Properties& properties) {
  if (adapter_path != object_path_)
    return;

  // DeviceFound can also be called to indicate that a device we've
  // paired with is now visible to the adapter, so check it's not already
  // in the list and just update if it is.
  BluetoothDevice* device;
  DevicesMap::iterator iter = devices_.find(address);
  if (iter == devices_.end()) {
    device = BluetoothDevice::CreateUnbound(this, &properties);
    devices_[address] = device;

    if (device->IsSupported())
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceAdded(this, device));
  } else {
    device = iter->second;
    device->Update(&properties, false);

    if (device->IsSupported() || device->IsPaired())
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceChanged(this, device));
  }
}

void BluetoothAdapter::DeviceDisappeared(const dbus::ObjectPath& adapter_path,
                                         const std::string& address) {
  if (adapter_path != object_path_)
    return;

  DevicesMap::iterator iter = devices_.find(address);
  if (iter == devices_.end())
    return;

  BluetoothDevice* device = iter->second;

  // DeviceDisappeared can also be called to indicate that a device we've
  // paired with is no longer visible to the adapter, so only delete
  // discovered devices.
  if (!device->IsPaired()) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, device));

    delete device;
    devices_.erase(iter);
  } else {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  }
}


// static
BluetoothAdapter* BluetoothAdapter::CreateDefaultAdapter() {
  BluetoothAdapter* adapter = new BluetoothAdapter;
  adapter->DefaultAdapter();
  return adapter;
}

// static
BluetoothAdapter* BluetoothAdapter::Create(const std::string& address) {
  BluetoothAdapter* adapter = new BluetoothAdapter;
  adapter->FindAdapter(address);
  return adapter;
}

}  // namespace chromeos

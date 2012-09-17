// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
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

namespace {

// Shared default adapter instance, we don't want to keep this class around
// if nobody is using it so use a WeakPtr and create the object when needed;
// since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<chromeos::BluetoothAdapter> >::Leaky
    default_adapter = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace chromeos {

BluetoothAdapter::BluetoothAdapter() : track_default_(false),
                                       powered_(false),
                                       discovering_(false),
                                       weak_ptr_factory_(this) {
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

bool BluetoothAdapter::IsPresent() const {
  return !object_path_.value().empty() && !address_.empty();
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

void BluetoothAdapter::TrackDefaultAdapter() {
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
  if (object_path_.value().empty()) {
    DVLOG(1) << "Adapter path initialized to " << adapter_path.value();
  } else if (object_path_.value() != adapter_path.value()) {
    DVLOG(1) << "Adapter path changed from " << object_path_.value()
             << " to " << adapter_path.value();

    RemoveAdapter();
  } else {
    DVLOG(1) << "Adapter address updated";
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
    DVLOG(1) << "Adapter address not yet known";
    return;
  }

  PoweredChanged(properties->powered.value());
  DiscoveringChanged(properties->discovering.value());
  DevicesChanged(properties->devices.value());

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, true));
}

void BluetoothAdapter::RemoveAdapter() {
  const bool adapter_was_present = IsPresent();

  DVLOG(1) << "Adapter lost.";
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

  } else if (property_name == properties->address.name()) {
    ChangeAdapter(object_path_);

  } else if (property_name == properties->name.name()) {
    name_ = properties->name.value();

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

  // The device may be already known to us, either because this is an update
  // to properties, or the device going from discovered to connected and
  // pairing gaining an object path in the process. In any case, we want
  // to update the existing object, not create a new one.
  DevicesMap::iterator iter = devices_.find(address);
  BluetoothDevice* device;
  const bool update_device = (iter != devices_.end());
  if (update_device) {
    device = iter->second;
  } else {
    device = BluetoothDevice::Create(this);
    devices_[address] = device;
  }

  const bool was_paired = device->IsPaired();
  if (!was_paired) {
    DVLOG(1) << "Assigned object path " << device_path.value() << " to device "
             << address;
    device->SetObjectPath(device_path);
  }
  device->Update(properties, true);

  // Don't send a duplicate added event for supported devices that were
  // previously visible or for already paired devices, send a changed
  // event instead. We always send one event or the other since we always
  // inform observers about paired devices whether or not they're supported.
  if (update_device && (device->IsSupported() || was_paired)) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  } else {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device));
  }
}

void BluetoothAdapter::ClearDevices() {
  DevicesMap replace;
  devices_.swap(replace);
  for (DevicesMap::iterator iter = replace.begin();
       iter != replace.end(); ++iter) {
    BluetoothDevice* device = iter->second;
    if (device->IsSupported() || device->IsPaired())
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

    if (device->object_path_ != device_path)
      continue;

    // DeviceRemoved can also be called to indicate a device that is visible
    // during discovery has disconnected, but it is still visible to the
    // adapter, so don't remove in that case and only clear the object path.
    if (!device->IsVisible()) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device));

      DVLOG(1) << "Removed device " << device->address();

      delete device;
      devices_.erase(temp);
    } else {
      DVLOG(1) << "Removed object path from device " << device->address();
      device->RemoveObjectPath();

      // If the device is not supported then we want to act as if it was
      // removed, even though it is still visible to the adapter.
      if (!device->IsSupported()) {
        FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                          DeviceRemoved(this, device));
      } else {
        FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                          DeviceChanged(this, device));
      }
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
      if (device->IsSupported())
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
  // paired with is now visible to the adapter during discovery, in which
  // case we want to update the existing object, not create a new one.
  BluetoothDevice* device;
  DevicesMap::iterator iter = devices_.find(address);
  const bool update_device = (iter != devices_.end());
  if (update_device) {
    device = iter->second;
  } else {
    device = BluetoothDevice::Create(this);
    devices_[address] = device;
  }

  DVLOG(1) << "Device " << address << " is visible to the adapter";
  device->SetVisible(true);
  device->Update(&properties, false);

  // Don't send a duplicated added event for duplicate signals for supported
  // devices that were previously visible (should never happen) or for already
  // paired devices, send a changed event instead. We do not inform observers
  // if we find or update an unconnected and unsupported device.
  if (update_device && (device->IsSupported() || device->IsPaired())) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  } else if (device->IsSupported()) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device));
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
  // paired with is no longer visible to the adapter, so don't remove
  // in that case and only clear the visible flag.
  if (!device->IsPaired()) {
    if (device->IsSupported())
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device));

    DVLOG(1) << "Discovered device " << device->address()
             << " is no longer visible to the adapter";

    delete device;
    devices_.erase(iter);
  } else {
    DVLOG(1) << "Paired device " << device->address()
             << " is no longer visible to the adapter";
    device->SetVisible(false);

    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceChanged(this, device));
  }
}


// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::DefaultAdapter() {
  if (!default_adapter.Get().get()) {
    BluetoothAdapter* new_adapter = new BluetoothAdapter;
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
    default_adapter.Get()->TrackDefaultAdapter();
  }

  return scoped_refptr<BluetoothAdapter>(default_adapter.Get());
}

// static
BluetoothAdapter* BluetoothAdapter::Create(const std::string& address) {
  BluetoothAdapter* adapter = new BluetoothAdapter;
  adapter->FindAdapter(address);
  return adapter;
}

}  // namespace chromeos

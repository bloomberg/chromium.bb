// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_property.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

BluetoothDeviceClient::Properties::Properties(dbus::ObjectProxy* object_proxy,
                                              PropertyChangedCallback callback)
    : BluetoothPropertySet(object_proxy,
                           bluetooth_device::kBluetoothDeviceInterface,
                           callback) {
  RegisterProperty(bluetooth_device::kAddressProperty, &address);
  RegisterProperty(bluetooth_device::kNameProperty, &name);
  RegisterProperty(bluetooth_device::kVendorProperty, &vendor);
  RegisterProperty(bluetooth_device::kProductProperty, &product);
  RegisterProperty(bluetooth_device::kVersionProperty, &version);
  RegisterProperty(bluetooth_device::kIconProperty, &icon);
  RegisterProperty(bluetooth_device::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_device::kUUIDsProperty, &uuids);
  RegisterProperty(bluetooth_device::kServicesProperty, &services);
  RegisterProperty(bluetooth_device::kPairedProperty, &paired);
  RegisterProperty(bluetooth_device::kConnectedProperty, &connected);
  RegisterProperty(bluetooth_device::kTrustedProperty, &trusted);
  RegisterProperty(bluetooth_device::kBlockedProperty, &blocked);
  RegisterProperty(bluetooth_device::kAliasProperty, &alias);
  RegisterProperty(bluetooth_device::kNodesProperty, &nodes);
  RegisterProperty(bluetooth_device::kAdapterProperty, &adapter);
  RegisterProperty(bluetooth_device::kLegacyPairingProperty, &legacy_pairing);
}

BluetoothDeviceClient::Properties::~Properties() {
}


// The BluetoothDeviceClient implementation used in production.
class BluetoothDeviceClientImpl: public BluetoothDeviceClient,
                                 private BluetoothAdapterClient::Observer {
 public:
  BluetoothDeviceClientImpl(dbus::Bus* bus,
                            BluetoothAdapterClient* adapter_client)
      : weak_ptr_factory_(this),
        bus_(bus) {
    DVLOG(1) << "Creating BluetoothDeviceClientImpl";

    DCHECK(adapter_client);
    adapter_client->AddObserver(this);
  }

  virtual ~BluetoothDeviceClientImpl() {
    // Clean up Properties structures
    for (ObjectMap::iterator iter = object_map_.begin();
         iter != object_map_.end(); ++iter) {
      Object object = iter->second;
      Properties* properties = object.second;
      delete properties;
    }
  }

  // BluetoothDeviceClient override.
  virtual void AddObserver(BluetoothDeviceClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothDeviceClient override.
  virtual void RemoveObserver(BluetoothDeviceClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothDeviceClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return GetObject(object_path).second;
  }

 private:
  // We maintain a collection of dbus object proxies and properties structures
  // for each device.
  typedef std::pair<dbus::ObjectProxy*, Properties*> Object;
  typedef std::map<const dbus::ObjectPath, Object> ObjectMap;
  ObjectMap object_map_;

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceCreated(const dbus::ObjectPath& adapter_path,
                             const dbus::ObjectPath& object_path) OVERRIDE {
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceRemoved(const dbus::ObjectPath& adapter_path,
                             const dbus::ObjectPath& object_path) OVERRIDE {
    RemoveObject(object_path);
  }

  // Ensures that we have an object proxy and properties structure for
  // a device with object path |object_path|, creating it if not and
  // storing it in our |object_map_| map.
  Object GetObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end())
      return iter->second;

    // Create the object proxy.
    DCHECK(bus_);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        bluetooth_device::kBluetoothDeviceServiceName, object_path);

    // Create the properties structure.
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&BluetoothDeviceClientImpl::PropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    properties->ConnectSignals();
    properties->GetAll();

    Object object = std::make_pair(object_proxy, properties);
    object_map_[object_path] = object;
    return object;
  }

  // Removes the dbus object proxy and properties for the device with
  // dbus object path |object_path| from our |object_map_| map.
  void RemoveObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end()) {
      // Clean up the Properties structure.
      Object object = iter->second;
      Properties* properties = object.second;
      delete properties;

      object_map_.erase(iter);
    }
  }

  // Returns a pointer to the object proxy for |object_path|, creating
  // it if necessary.
  dbus::ObjectProxy* GetObjectProxy(const dbus::ObjectPath& object_path) {
    return GetObject(object_path).first;
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void PropertyChanged(const dbus::ObjectPath& object_path,
                       const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      PropertyChanged(object_path, property_name));
  }

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  base::WeakPtrFactory<BluetoothDeviceClientImpl> weak_ptr_factory_;

  dbus::Bus* bus_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothDeviceClient::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClientImpl);
};

// The BluetoothDeviceClient implementation used on Linux desktop, which does
// nothing.
class BluetoothDeviceClientStubImpl : public BluetoothDeviceClient {
 public:
  // BluetoothDeviceClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothDeviceClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothDeviceClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    VLOG(1) << "GetProperties: " << object_path.value();
    return NULL;
  }
};

BluetoothDeviceClient::BluetoothDeviceClient() {
}

BluetoothDeviceClient::~BluetoothDeviceClient() {
}

BluetoothDeviceClient* BluetoothDeviceClient::Create(
    dbus::Bus* bus,
    BluetoothAdapterClient* adapter_client) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new BluetoothDeviceClientImpl(bus, adapter_client);
  } else {
    return new BluetoothDeviceClientStubImpl();
  }
}

}  // namespace chromeos

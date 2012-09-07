// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_node_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

BluetoothNodeClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : BluetoothPropertySet(object_proxy,
                           bluetooth_node::kBluetoothNodeInterface,
                           callback) {
  RegisterProperty(bluetooth_node::kNameProperty, &name);
  RegisterProperty(bluetooth_node::kDeviceProperty, &device);
}

BluetoothNodeClient::Properties::~Properties() {
}


// The BluetoothNodeClient implementation used in production.
class BluetoothNodeClientImpl: public BluetoothNodeClient,
                               private BluetoothDeviceClient::Observer {
 public:
  BluetoothNodeClientImpl(dbus::Bus* bus,
                          BluetoothDeviceClient* device_client)
      : bus_(bus),
        weak_ptr_factory_(this) {
    DVLOG(1) << "Creating BluetoothNodeClientImpl";

    DCHECK(device_client);
    device_client->AddObserver(this);
  }

  virtual ~BluetoothNodeClientImpl() {
    // Clean up Properties structures
    for (ObjectMap::iterator iter = object_map_.begin();
         iter != object_map_.end(); ++iter) {
      Object object = iter->second;
      Properties* properties = object.second;
      delete properties;
    }
  }

  // BluetoothNodeClient override.
  virtual void AddObserver(BluetoothNodeClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothNodeClient override.
  virtual void RemoveObserver(BluetoothNodeClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothNodeClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return GetObject(object_path).second;
  }

 private:
  // We maintain a collection of dbus object proxies and properties structures
  // for each node binding.
  typedef std::pair<dbus::ObjectProxy*, Properties*> Object;
  typedef std::map<const dbus::ObjectPath, Object> ObjectMap;
  ObjectMap object_map_;

  // BluetoothDeviceClient::Observer override.
  virtual void NodeCreated(const dbus::ObjectPath& device_path,
                           const dbus::ObjectPath& object_path) OVERRIDE {
  }

  // BluetoothDeviceClient::Observer override.
  virtual void NodeRemoved(const dbus::ObjectPath& device_path,
                           const dbus::ObjectPath& object_path) OVERRIDE {
    RemoveObject(object_path);
  }

  // Ensures that we have an object proxy and properties structure for
  // a node binding with object path |object_path|, creating it if not and
  // storing it in our |object_map_| map.
  Object GetObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end())
      return iter->second;

    // Create the object proxy.
    DCHECK(bus_);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        bluetooth_node::kBluetoothNodeServiceName, object_path);

    // Create the properties structure.
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&BluetoothNodeClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    properties->ConnectSignals();
    properties->GetAll();

    Object object = std::make_pair(object_proxy, properties);
    object_map_[object_path] = object;
    return object;
  }

  // Removes the dbus object proxy and properties for the node binding with
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
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothNodeClient::Observer, observers_,
                      NodePropertyChanged(object_path, property_name));
  }

  dbus::Bus* bus_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothNodeClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothNodeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothNodeClientImpl);
};

// The BluetoothNodeClient implementation used on Linux desktop, which does
// nothing.
class BluetoothNodeClientStubImpl : public BluetoothNodeClient {
 public:
  // BluetoothNodeClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothNodeClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothNodeClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    VLOG(1) << "GetProperties: " << object_path.value();
    return NULL;
  }
};

BluetoothNodeClient::BluetoothNodeClient() {
}

BluetoothNodeClient::~BluetoothNodeClient() {
}

BluetoothNodeClient* BluetoothNodeClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    BluetoothDeviceClient* adapter_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothNodeClientImpl(bus, adapter_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothNodeClientStubImpl();
}

}  // namespace chromeos

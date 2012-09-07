// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_input_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char BluetoothInputClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

BluetoothInputClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : BluetoothPropertySet(object_proxy,
                           bluetooth_input::kBluetoothInputInterface,
                           callback) {
  RegisterProperty(bluetooth_input::kConnectedProperty, &connected);
}

BluetoothInputClient::Properties::~Properties() {
}


// The BluetoothInputClient implementation used in production.
class BluetoothInputClientImpl: public BluetoothInputClient,
                                private BluetoothAdapterClient::Observer {
 public:
  BluetoothInputClientImpl(dbus::Bus* bus,
                           BluetoothAdapterClient* adapter_client)
      : bus_(bus),
      weak_ptr_factory_(this) {
    DVLOG(1) << "Creating BluetoothInputClientImpl";

    DCHECK(adapter_client);
    adapter_client->AddObserver(this);
  }

  virtual ~BluetoothInputClientImpl() {
    // Clean up Properties structures
    for (ObjectMap::iterator iter = object_map_.begin();
         iter != object_map_.end(); ++iter) {
      Object object = iter->second;
      Properties* properties = object.second;
      delete properties;
    }
  }

  // BluetoothInputClient override.
  virtual void AddObserver(BluetoothInputClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothInputClient override.
  virtual void RemoveObserver(BluetoothInputClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothInputClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return GetObject(object_path).second;
  }

  // BluetoothInputClient override.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const ConnectCallback& callback,
                       const ConnectErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_input::kBluetoothInputInterface,
        bluetooth_input::kConnect);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothInputClientImpl::OnConnect,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   callback),
        base::Bind(&BluetoothInputClientImpl::OnConnectError,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   error_callback));
  }

  // BluetoothInputClient override.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const InputCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_input::kBluetoothInputInterface,
        bluetooth_input::kDisconnect);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothInputClientImpl::OnDisconnect,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

 private:
  // We maintain a collection of dbus object proxies and properties structures
  // for each input device.
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
  // an input device with object path |object_path|, creating it if not and
  // storing it in our |object_map_| map.
  Object GetObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end())
      return iter->second;

    // Create the object proxy.
    DCHECK(bus_);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        bluetooth_input::kBluetoothInputServiceName, object_path);

    // Create the properties structure.
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&BluetoothInputClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    properties->ConnectSignals();
    properties->GetAll();

    Object object = std::make_pair(object_proxy, properties);
    object_map_[object_path] = object;
    return object;
  }

  // Removes the dbus object proxy and properties for the input device with
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
    FOR_EACH_OBSERVER(BluetoothInputClient::Observer, observers_,
                      InputPropertyChanged(object_path, property_name));
  }

  // Called when a response for Connect() is received.
  void OnConnect(const dbus::ObjectPath& object_path,
                 const ConnectCallback& callback,
                 dbus::Response* response) {
    DCHECK(response);
    callback.Run(object_path);
  }

  // Called when an error for Connect() is received.
  void OnConnectError(const dbus::ObjectPath& object_path,
                      const ConnectErrorCallback& error_callback,
                      dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      error_message = reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(object_path, error_name, error_message);
  }

  // Called when a response for Disconnect() is received.
  void OnDisconnect(const dbus::ObjectPath& object_path,
                    const InputCallback& callback,
                    dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnDisconnect: failed.";
    callback.Run(object_path, response);
  }

  dbus::Bus* bus_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothInputClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothInputClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothInputClientImpl);
};

// The BluetoothInputClient implementation used on Linux desktop, which does
// nothing.
class BluetoothInputClientStubImpl : public BluetoothInputClient {
 public:
  // BluetoothInputClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothInputClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothInputClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    VLOG(1) << "GetProperties: " << object_path.value();
    return NULL;
  }

  // BluetoothInputClient override.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const ConnectCallback& callback,
                       const ConnectErrorCallback& error_callback) OVERRIDE {
    VLOG(1) << "Connect: " << object_path.value();
    error_callback.Run(object_path, "", "");
  }

  // BluetoothInputClient override.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const InputCallback& callback) OVERRIDE {
    VLOG(1) << "Disconnect: " << object_path.value();
    callback.Run(object_path, false);
  }
};

BluetoothInputClient::BluetoothInputClient() {
}

BluetoothInputClient::~BluetoothInputClient() {
}

BluetoothInputClient* BluetoothInputClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    BluetoothAdapterClient* adapter_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothInputClientImpl(bus, adapter_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothInputClientStubImpl();
}

}  // namespace chromeos

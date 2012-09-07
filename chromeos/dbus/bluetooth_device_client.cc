// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_device_client.h"

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

BluetoothDeviceClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
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
      : bus_(bus),
        weak_ptr_factory_(this) {
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

  // BluetoothDeviceClient override.
  virtual void DiscoverServices(const dbus::ObjectPath& object_path,
                                const std::string& pattern,
                                const ServicesCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kDiscoverServices);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pattern);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothDeviceClientImpl::OnDiscoverServices,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothDeviceClient override.
  virtual void CancelDiscovery(const dbus::ObjectPath& object_path,
                               const DeviceCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kCancelDiscovery);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothDeviceClientImpl::OnCancelDiscovery,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothDeviceClient override.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const DeviceCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kDisconnect);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothDeviceClientImpl::OnDisconnect,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothDeviceClient override.
  virtual void CreateNode(const dbus::ObjectPath& object_path,
                          const std::string& uuid,
                          const NodeCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kCreateNode);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(uuid);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothDeviceClientImpl::OnCreateNode,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothDeviceClient override.
  virtual void RemoveNode(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& node_path,
                          const DeviceCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kRemoveNode);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(node_path);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothDeviceClientImpl::OnRemoveNode,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
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

    object_proxy->ConnectToSignal(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kDisconnectRequestedSignal,
        base::Bind(&BluetoothDeviceClientImpl::DisconnectRequestedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothDeviceClientImpl::DisconnectRequestedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    object_proxy->ConnectToSignal(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kNodeCreatedSignal,
        base::Bind(&BluetoothDeviceClientImpl::NodeCreatedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothDeviceClientImpl::NodeCreatedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    object_proxy->ConnectToSignal(
        bluetooth_device::kBluetoothDeviceInterface,
        bluetooth_device::kNodeRemovedSignal,
        base::Bind(&BluetoothDeviceClientImpl::NodeRemovedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothDeviceClientImpl::NodeRemovedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    // Create the properties structure.
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&BluetoothDeviceClientImpl::OnPropertyChanged,
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
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      DevicePropertyChanged(object_path, property_name));
  }

  // Called by dbus:: when a DisconnectRequested signal is received.
  void DisconnectRequestedReceived(const dbus::ObjectPath& object_path,
                                   dbus::Signal* signal) {
    DCHECK(signal);

    DVLOG(1) << object_path.value() << ": Disconnect requested.";
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      DisconnectRequested(object_path));
  }

  // Called by dbus:: when the DisconnectRequested signal is initially
  // connected.
  void DisconnectRequestedConnected(const dbus::ObjectPath& object_path,
                                    const std::string& interface_name,
                                    const std::string& signal_name,
                                    bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to "
                                 "DisconnectRequested signal.";
  }

  // Called by dbus:: when a NodeCreated signal is received.
  void NodeCreatedReceived(const dbus::ObjectPath& object_path,
                           dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath node_path;
    if (!reader.PopObjectPath(&node_path)) {
      LOG(WARNING) << object_path.value()
                   << ": NodeCreated signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Node created: "
             << node_path.value();
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      NodeCreated(object_path, node_path));
  }

  // Called by dbus:: when the NodeCreated signal is initially connected.
  void NodeCreatedConnected(const dbus::ObjectPath& object_path,
                            const std::string& interface_name,
                            const std::string& signal_name,
                            bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to NodeCreated signal.";
  }

  // Called by dbus:: when a NodeRemoved signal is received.
  void NodeRemovedReceived(const dbus::ObjectPath& object_path,
                           dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath node_path;
    if (!reader.PopObjectPath(&node_path)) {
      LOG(WARNING) << object_path.value()
                   << ": NodeRemoved signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Node removed: "
             << node_path.value();
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      NodeRemoved(object_path, node_path));
  }

  // Called by dbus:: when the NodeRemoved signal is initially connected.
  void NodeRemovedConnected(const dbus::ObjectPath& object_path,
                            const std::string& interface_name,
                            const std::string& signal_name,
                            bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to NodeRemoved signal.";
  }

  // Called when a response for DiscoverServices() is received.
  void OnDiscoverServices(const dbus::ObjectPath& object_path,
                          const ServicesCallback& callback,
                          dbus::Response* response) {
    // Parse response.
    bool success = false;
    ServiceMap services;
    if (response != NULL) {
      dbus::MessageReader reader(response);

      dbus::MessageReader array_reader(NULL);
      if (!reader.PopArray(&array_reader)) {
        LOG(WARNING) << "DiscoverServices response has incorrect parameters: "
                     << response->ToString();
      } else {
        while (array_reader.HasMoreData()) {
          dbus::MessageReader dict_entry_reader(NULL);
          uint32 key = 0;
          std::string value;
          if (!array_reader.PopDictEntry(&dict_entry_reader)
              || !dict_entry_reader.PopUint32(&key)
              || !dict_entry_reader.PopString(&value)) {
            LOG(WARNING) << "DiscoverServices response has "
                            "incorrect parameters: " << response->ToString();
          } else {
            services[key] = value;
          }
        }

        success = true;
      }
    } else {
      LOG(WARNING) << "Failed to discover services.";
    }

    // Notify client.
    callback.Run(object_path, services, success);
  }

  // Called when a response for CancelDiscovery() is received.
  void OnCancelDiscovery(const dbus::ObjectPath& object_path,
                         const DeviceCallback& callback,
                         dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnCancelDiscovery: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for Disconnect() is received.
  void OnDisconnect(const dbus::ObjectPath& object_path,
                    const DeviceCallback& callback,
                    dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnDisconnect: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for CreateNode() is received.
  void OnCreateNode(const dbus::ObjectPath& object_path,
                    const NodeCallback& callback,
                    dbus::Response* response) {
    // Parse response.
    bool success = false;
    dbus::ObjectPath node_path;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&node_path)) {
        LOG(WARNING) << "CreateNode response has incorrect parameters: "
                     << response->ToString();
      } else {
        success = true;
      }
    } else {
      LOG(WARNING) << "Failed to create node.";
    }

    // Notify client.
    callback.Run(node_path, success);
  }

  // Called when a response for RemoveNode() is received.
  void OnRemoveNode(const dbus::ObjectPath& object_path,
                    const DeviceCallback& callback,
                    dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnRemoveNode: failed.";
    callback.Run(object_path, response);
  }

  dbus::Bus* bus_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothDeviceClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClientImpl);
};

// The BluetoothDeviceClient implementation used on Linux desktop, which does
// nothing.
class BluetoothDeviceClientStubImpl : public BluetoothDeviceClient {
 public:
  struct Properties : public BluetoothDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothDeviceClient::Properties(NULL, callback) {
    }

    virtual ~Properties() {
    }

    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE {
      VLOG(1) << "Get " << property->name();
      callback.Run(false);
    }

    virtual void GetAll() OVERRIDE {
      VLOG(1) << "GetAll";
    }

    virtual void Set(dbus::PropertyBase *property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE {
      VLOG(1) << "Set " << property->name();
      callback.Run(false);
    }
  };

  BluetoothDeviceClientStubImpl() {
    dbus::ObjectPath dev0("/fake/hci0/dev0");

    Properties* properties = new Properties(base::Bind(
        &BluetoothDeviceClientStubImpl::OnPropertyChanged,
        base::Unretained(this),
        dev0));
    properties->address.ReplaceValue("00:11:22:33:44:55");
    properties->name.ReplaceValue("Fake Device");
    properties->paired.ReplaceValue(true);
    properties->trusted.ReplaceValue(true);

    properties_map_[dev0] = properties;
  }

  virtual ~BluetoothDeviceClientStubImpl() {
    // Clean up Properties structures
    STLDeleteValues(&properties_map_);
  }

  // BluetoothDeviceClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // BluetoothDeviceClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // BluetoothDeviceClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    VLOG(1) << "GetProperties: " << object_path.value();
    PropertiesMap::iterator iter = properties_map_.find(object_path);
    if (iter != properties_map_.end())
      return iter->second;
    return NULL;
  }

  // BluetoothDeviceClient override.
  virtual void DiscoverServices(const dbus::ObjectPath& object_path,
                                const std::string& pattern,
                                const ServicesCallback& callback) OVERRIDE {
    VLOG(1) << "DiscoverServices: " << object_path.value() << " " << pattern;

    ServiceMap services;
    callback.Run(object_path, services, false);
  }

  // BluetoothDeviceClient override.
  virtual void CancelDiscovery(const dbus::ObjectPath& object_path,
                               const DeviceCallback& callback) OVERRIDE {
    VLOG(1) << "CancelDiscovery: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothDeviceClient override.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const DeviceCallback& callback) OVERRIDE {
    VLOG(1) << "Disconnect: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothDeviceClient override.
  virtual void CreateNode(const dbus::ObjectPath& object_path,
                          const std::string& uuid,
                          const NodeCallback& callback) OVERRIDE {
    VLOG(1) << "CreateNode: " << object_path.value() << " " << uuid;
    callback.Run(dbus::ObjectPath(), false);
  }

  // BluetoothDeviceClient override.
  virtual void RemoveNode(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& node_path,
                          const DeviceCallback& callback) OVERRIDE {
    VLOG(1) << "RemoveNode: " << object_path.value()
            << " " << node_path.value();
    callback.Run(object_path, false);
  }

 private:
  void OnPropertyChanged(dbus::ObjectPath object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                      DevicePropertyChanged(object_path, property_name));
  }

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we typedef.
  typedef std::map<const dbus::ObjectPath, Properties *> PropertiesMap;
  PropertiesMap properties_map_;
};

BluetoothDeviceClient::BluetoothDeviceClient() {
}

BluetoothDeviceClient::~BluetoothDeviceClient() {
}

BluetoothDeviceClient* BluetoothDeviceClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    BluetoothAdapterClient* adapter_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothDeviceClientImpl(bus, adapter_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothDeviceClientStubImpl();
}

}  // namespace chromeos

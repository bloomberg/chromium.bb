// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_adapter_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// The |CreatePairedDevice| DBus call needs a longer timeout than the default
// in order to allow BlueZ to timeout this call first. See crosbug.com/37387.
const int kCreatePairedDeviceTimeoutMs = 120 * 1000;

} // namespace

namespace chromeos {

const char BluetoothAdapterClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothAdapterClient::kBadResponseError[] =
    "org.chromium.Error.BadResponse";

BluetoothAdapterClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : BluetoothPropertySet(object_proxy,
                           bluetooth_adapter::kBluetoothAdapterInterface,
                           callback) {
  RegisterProperty(bluetooth_adapter::kAddressProperty, &address);
  RegisterProperty(bluetooth_adapter::kNameProperty, &name);
  RegisterProperty(bluetooth_adapter::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_adapter::kPoweredProperty, &powered);
  RegisterProperty(bluetooth_adapter::kDiscoverableProperty, &discoverable);
  RegisterProperty(bluetooth_adapter::kPairableProperty, &pairable);
  RegisterProperty(bluetooth_adapter::kPairableTimeoutProperty,
                   &pairable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoverableTimeoutProperty,
                   &discoverable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoveringProperty, &discovering);
  RegisterProperty(bluetooth_adapter::kDevicesProperty, &devices);
  RegisterProperty(bluetooth_adapter::kUUIDsProperty, &uuids);
}

BluetoothAdapterClient::Properties::~Properties() {
}


// The BluetoothAdapterClient implementation used in production.
class BluetoothAdapterClientImpl: public BluetoothAdapterClient,
                                  private BluetoothManagerClient::Observer {
 public:
  explicit BluetoothAdapterClientImpl(dbus::Bus* bus,
                                      BluetoothManagerClient* manager_client)
      : bus_(bus),
        weak_ptr_factory_(this) {
    DVLOG(1) << "Creating BluetoothAdapterClientImpl";

    DCHECK(manager_client);
    manager_client->AddObserver(this);
  }

  virtual ~BluetoothAdapterClientImpl() {
    // Clean up Properties structures
    for (ObjectMap::iterator iter = object_map_.begin();
         iter != object_map_.end(); ++iter) {
      Object object = iter->second;
      Properties* properties = object.second;
      delete properties;
    }
  }

  // BluetoothAdapterClient override.
  virtual void AddObserver(BluetoothAdapterClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAdapterClient override.
  virtual void RemoveObserver(BluetoothAdapterClient::Observer* observer)
      OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothAdapterClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return GetObject(object_path).second;
  }

  // BluetoothAdapterClient override.
  virtual void RequestSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kRequestSession);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnRequestSession,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void ReleaseSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kReleaseSession);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnReleaseSession,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kStartDiscovery);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnStartDiscovery,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kStopDiscovery);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnStopDiscovery,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void FindDevice(const dbus::ObjectPath& object_path,
                          const std::string& address,
                          const DeviceCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kFindDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnFindDevice,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void CreateDevice(const dbus::ObjectPath& object_path,
                            const std::string& address,
                            const CreateDeviceCallback& callback,
                            const CreateDeviceErrorCallback& error_callback)
      OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kCreateDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnCreateDevice,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   callback, error_callback),
        base::Bind(&BluetoothAdapterClientImpl::OnCreateDeviceError,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   error_callback));
  }

  // BluetoothAdapterClient override.
  virtual void CreatePairedDevice(
      const dbus::ObjectPath& object_path, const std::string& address,
      const dbus::ObjectPath& agent_path, const std::string& capability,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kCreatePairedDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);
    writer.AppendObjectPath(agent_path);
    writer.AppendString(capability);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        kCreatePairedDeviceTimeoutMs,
        base::Bind(&BluetoothAdapterClientImpl::OnCreatePairedDevice,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   callback, error_callback),
        base::Bind(&BluetoothAdapterClientImpl::OnCreatePairedDeviceError,
                   weak_ptr_factory_.GetWeakPtr(), object_path,
                   error_callback));
  }

  // BluetoothAdapterClient override.
  virtual void CancelDeviceCreation(const dbus::ObjectPath& object_path,
                                    const std::string& address,
                                    const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kCancelDeviceCreation);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnCancelDeviceCreation,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kRemoveDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(device_path);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnRemoveDevice,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void RegisterAgent(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& agent_path,
                             const std::string& capability,
                             const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kRegisterAgent);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(agent_path);
    writer.AppendString(capability);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnRegisterAgent,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

  // BluetoothAdapterClient override.
  virtual void UnregisterAgent(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& agent_path,
                               const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kUnregisterAgent);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(agent_path);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothAdapterClientImpl::OnUnregisterAgent,
                   weak_ptr_factory_.GetWeakPtr(), object_path, callback));
  }

 private:
  // We maintain a collection of dbus object proxies and properties structures
  // for each adapter.
  typedef std::pair<dbus::ObjectProxy*, Properties*> Object;
  typedef std::map<const dbus::ObjectPath, Object> ObjectMap;
  ObjectMap object_map_;

  // BluetoothManagerClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE {
  }

  // BluetoothManagerClient::Observer override.
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    RemoveObject(object_path);
  }

  // Ensures that we have an object proxy and properties structure for
  // an adapter with object path |object_path|, creating it if not and
  // storing in our |object_map_| map.
  Object GetObject(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end())
      return iter->second;

    // Create the object proxy.
    DCHECK(bus_);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        bluetooth_adapter::kBluetoothAdapterServiceName, object_path);

    object_proxy->ConnectToSignal(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kDeviceCreatedSignal,
        base::Bind(&BluetoothAdapterClientImpl::DeviceCreatedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothAdapterClientImpl::DeviceCreatedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    object_proxy->ConnectToSignal(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kDeviceRemovedSignal,
        base::Bind(&BluetoothAdapterClientImpl::DeviceRemovedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothAdapterClientImpl::DeviceRemovedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    object_proxy->ConnectToSignal(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kDeviceFoundSignal,
        base::Bind(&BluetoothAdapterClientImpl::DeviceFoundReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothAdapterClientImpl::DeviceFoundConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    object_proxy->ConnectToSignal(
        bluetooth_adapter::kBluetoothAdapterInterface,
        bluetooth_adapter::kDeviceDisappearedSignal,
        base::Bind(&BluetoothAdapterClientImpl::DeviceDisappearedReceived,
                   weak_ptr_factory_.GetWeakPtr(), object_path),
        base::Bind(&BluetoothAdapterClientImpl::DeviceDisappearedConnected,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    // Create the properties structure.
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&BluetoothAdapterClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));

    properties->ConnectSignals();
    properties->GetAll();

    Object object = std::make_pair(object_proxy, properties);
    object_map_[object_path] = object;
    return object;
  }

  // Removes the dbus object proxy and properties for the adapter with
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
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterPropertyChanged(object_path, property_name));
  }

  // Called by dbus:: when a DeviceCreated signal is received.
  void DeviceCreatedReceived(const dbus::ObjectPath& object_path,
                             dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << object_path.value()
                   << ": DeviceCreated signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Device created: "
             << device_path.value();
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      DeviceCreated(object_path, device_path));
  }

  // Called by dbus:: when the DeviceCreated signal is initially connected.
  void DeviceCreatedConnected(const dbus::ObjectPath& object_path,
                              const std::string& interface_name,
                              const std::string& signal_name,
                              bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to DeviceCreated signal.";
  }

  // Called by dbus:: when a DeviceRemoved signal is received.
  void DeviceRemovedReceived(const dbus::ObjectPath& object_path,
                             dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << object_path.value()
                   << ": DeviceRemoved signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Device removed: "
             << device_path.value();
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      DeviceRemoved(object_path, device_path));
  }

  // Called by dbus:: when the DeviceRemoved signal is initially connected.
  void DeviceRemovedConnected(const dbus::ObjectPath& object_path,
                              const std::string& interface_name,
                              const std::string& signal_name,
                              bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to DeviceRemoved signal.";
  }

  // Called by dbus:: when a DeviceFound signal is received.
  void DeviceFoundReceived(const dbus::ObjectPath& object_path,
                           dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    std::string address;
    if (!reader.PopString(&address)) {
      LOG(WARNING) << object_path.value()
                   << ": DeviceFound signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    // Create device properties structure without an attached object_proxy
    // and a NULL callback; value() functions will work on this, but not
    // Get() or Set() calls.
    BluetoothDeviceClient::Properties device_properties(
        NULL, BluetoothDeviceClient::Properties::PropertyChangedCallback());
    if (!device_properties.UpdatePropertiesFromReader(&reader)) {
      LOG(WARNING) << object_path.value()
                   << ": DeviceFound signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Device found: " << address;
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      DeviceFound(object_path, address, device_properties));
  }

  // Called by dbus:: when the DeviceFound signal is initially connected.
  void DeviceFoundConnected(const dbus::ObjectPath& object_path,
                            const std::string& interface_name,
                            const std::string& signal_name,
                            bool success) {
    LOG_IF(WARNING, !success) << object_path.value()
                              << ": Failed to connect to DeviceFound signal.";
  }

  // Called by dbus:: when a DeviceDisappeared signal is received.
  void DeviceDisappearedReceived(const dbus::ObjectPath& object_path,
                                 dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    std::string address;
    if (!reader.PopString(&address)) {
      LOG(WARNING) << object_path.value()
                   << ": DeviceDisappeared signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }

    DVLOG(1) << object_path.value() << ": Device disappeared: " << address;
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      DeviceDisappeared(object_path, address));
  }

  // Called by dbus:: when the DeviceDisappeared signal is initially connected.
  void DeviceDisappearedConnected(const dbus::ObjectPath& object_path,
                                  const std::string& interface_name,
                                  const std::string& signal_name,
                                  bool success) {
    LOG_IF(WARNING, !success)
        << object_path.value()
        << ": Failed to connect to DeviceDisappeared signal.";
  }

  // Called when a response for RequestSession() is received.
  void OnRequestSession(const dbus::ObjectPath& object_path,
                        const AdapterCallback& callback,
                        dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnRequestSession: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for ReleaseSession() is received.
  void OnReleaseSession(const dbus::ObjectPath& object_path,
                        const AdapterCallback& callback,
                        dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnReleaseSession: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for StartDiscovery() is received.
  void OnStartDiscovery(const dbus::ObjectPath& object_path,
                        const AdapterCallback& callback,
                        dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnStartDiscovery: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for StopDiscovery() is received.
  void OnStopDiscovery(const dbus::ObjectPath& object_path,
                       const AdapterCallback& callback,
                       dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnStopDiscovery: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for FindDevice() is received.
  void OnFindDevice(const dbus::ObjectPath& object_path,
                    const DeviceCallback& callback,
                    dbus::Response* response) {
    // Parse response.
    bool success = false;
    dbus::ObjectPath device_path;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&device_path)) {
        LOG(WARNING) << "FindDevice response has incorrect parameters: "
                     << response->ToString();
      } else {
        success = true;
      }
    } else {
      LOG(WARNING) << "Failed to find device.";
    }

    // Notify client.
    callback.Run(device_path, success);
  }

  // Called when a response for CreateDevice() is received.
  void OnCreateDevice(const dbus::ObjectPath& object_path,
                      const CreateDeviceCallback& callback,
                      const CreateDeviceErrorCallback& error_callback,
                      dbus::Response* response) {
    // Parse response.
    DCHECK(response);
    dbus::ObjectPath device_path;
    dbus::MessageReader reader(response);
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "CreateDevice response has incorrect parameters: "
                   << response->ToString();
      error_callback.Run(kBadResponseError, "");
      return;
    }

    // Notify client.
    callback.Run(device_path);
  }

  // Called when an error for CreateDevice() is received.
  void OnCreateDeviceError(const dbus::ObjectPath& object_path,
                           const CreateDeviceErrorCallback& error_callback,
                           dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(error_name, error_message);
  }

  // Called when a response for CreatePairedDevice() is received.
  void OnCreatePairedDevice(const dbus::ObjectPath& object_path,
                            const CreateDeviceCallback& callback,
                            const CreateDeviceErrorCallback& error_callback,
                            dbus::Response* response) {
    // Parse response.
    DCHECK(response);
    dbus::ObjectPath device_path;
    dbus::MessageReader reader(response);
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "CreatePairedDevice response has incorrect parameters: "
                   << response->ToString();
      error_callback.Run(kBadResponseError, "");
      return;
    }

    // Notify client.
    callback.Run(device_path);
  }

  // Called when an error for CreatePairedDevice() is received.
  void OnCreatePairedDeviceError(
      const dbus::ObjectPath& object_path,
      const CreateDeviceErrorCallback& error_callback,
      dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(error_name, error_message);
  }

  // Called when a response for CancelDeviceCreation() is received.
  void OnCancelDeviceCreation(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback,
                              dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnCancelDeviceCreation: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for RemoveDevice() is received.
  void OnRemoveDevice(const dbus::ObjectPath& object_path,
                      const AdapterCallback& callback,
                      dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnRemoveDevice: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for RegisterAgent() is received.
  void OnRegisterAgent(const dbus::ObjectPath& object_path,
                       const AdapterCallback& callback,
                       dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnRegisterAgent: failed.";
    callback.Run(object_path, response);
  }

  // Called when a response for UnregisterAgent() is received.
  void OnUnregisterAgent(const dbus::ObjectPath& object_path,
                         const AdapterCallback& callback,
                         dbus::Response* response) {
    LOG_IF(WARNING, !response) << object_path.value()
                               << ": OnUnregisterAgent: failed.";
    callback.Run(object_path, response);
  }

  dbus::Bus* bus_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapterClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterClientImpl);
};

// The BluetoothAdapterClient implementation used on Linux desktop, which does
// nothing.
class BluetoothAdapterClientStubImpl : public BluetoothAdapterClient {
 public:
  struct Properties : public BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothAdapterClient::Properties(NULL, callback) {
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
      if (property->name() == "Powered") {
        property->ReplaceValueWithSetValue();
        NotifyPropertyChanged(property->name());
        callback.Run(true);
      } else {
        callback.Run(false);
      }
    }
  };

  BluetoothAdapterClientStubImpl() {
    properties_.reset(new Properties(base::Bind(
        &BluetoothAdapterClientStubImpl::OnPropertyChanged,
        base::Unretained(this))));

    properties_->address.ReplaceValue("hci0");
    properties_->name.ReplaceValue("Fake Adapter");
    properties_->pairable.ReplaceValue(true);

    std::vector<dbus::ObjectPath> devices;
    devices.push_back(dbus::ObjectPath("/fake/hci0/dev0"));
    properties_->devices.ReplaceValue(devices);
  }

  // BluetoothAdapterClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // BluetoothAdapterClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // BluetoothAdapterClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    VLOG(1) << "GetProperties: " << object_path.value();
    if (object_path.value() == "/fake/hci0")
      return properties_.get();
    else
      return NULL;
  }

  // BluetoothAdapterClient override.
  virtual void RequestSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "RequestSession: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void ReleaseSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "ReleaseSession: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "StartDiscovery: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "StopDiscovery: " << object_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void FindDevice(const dbus::ObjectPath& object_path,
                          const std::string& address,
                          const DeviceCallback& callback) OVERRIDE {
    VLOG(1) << "FindDevice: " << object_path.value() << " " << address;
    callback.Run(dbus::ObjectPath(), false);
  }

  // BluetoothAdapterClient override.
  virtual void CreateDevice(const dbus::ObjectPath& object_path,
                            const std::string& address,
                            const CreateDeviceCallback& callback,
                            const CreateDeviceErrorCallback& error_callback)
      OVERRIDE {
    VLOG(1) << "CreateDevice: " << object_path.value() << " " << address;
    error_callback.Run("", "");
  }

  // BluetoothAdapterClient override.
  virtual void CreatePairedDevice(
      const dbus::ObjectPath& object_path, const std::string& address,
      const dbus::ObjectPath& agent_path, const std::string& capability,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) OVERRIDE {
    VLOG(1) << "CreatePairedDevice: " << object_path.value() << " " << address
            << " " << agent_path.value() << " " << capability;
    error_callback.Run("", "");
  }

  // BluetoothAdapterClient override.
  virtual void CancelDeviceCreation(const dbus::ObjectPath& object_path,
                                    const std::string& address,
                                    const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "CancelDeviceCreation: " << object_path.value()
            << " " << address;
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "RemoveDevice: " << object_path.value()
            << " " << device_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void RegisterAgent(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& agent_path,
                             const std::string& capability,
                             const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "RegisterAgent: " << object_path.value()
            << " " << agent_path.value();
    callback.Run(object_path, false);
  }

  // BluetoothAdapterClient override.
  virtual void UnregisterAgent(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& agent_path,
                               const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "UnregisterAgent: " << object_path.value()
            << " " << agent_path.value();
    callback.Run(object_path, false);
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterPropertyChanged(dbus::ObjectPath("/fake/hci0"),
                                             property_name));
  }

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  scoped_ptr<Properties> properties_;
};

BluetoothAdapterClient::BluetoothAdapterClient() {
}

BluetoothAdapterClient::~BluetoothAdapterClient() {
}

BluetoothAdapterClient* BluetoothAdapterClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    BluetoothManagerClient* manager_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothAdapterClientImpl(bus, manager_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothAdapterClientStubImpl();
}

}  // namespace chromeos

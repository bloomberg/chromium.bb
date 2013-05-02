// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/experimental_bluetooth_adapter_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char ExperimentalBluetoothAdapterClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char ExperimentalBluetoothAdapterClient::kUnknownAdapterError[] =
    "org.chromium.Error.UnknownAdapter";

ExperimentalBluetoothAdapterClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_adapter::kAddressProperty, &address);
  RegisterProperty(bluetooth_adapter::kNameProperty, &name);
  RegisterProperty(bluetooth_adapter::kAliasProperty, &alias);
  RegisterProperty(bluetooth_adapter::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_adapter::kPoweredProperty, &powered);
  RegisterProperty(bluetooth_adapter::kDiscoverableProperty, &discoverable);
  RegisterProperty(bluetooth_adapter::kPairableProperty, &pairable);
  RegisterProperty(bluetooth_adapter::kPairableTimeoutProperty,
                   &pairable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoverableTimeoutProperty,
                   &discoverable_timeout);
  RegisterProperty(bluetooth_adapter::kDiscoveringProperty, &discovering);
  RegisterProperty(bluetooth_adapter::kUUIDsProperty, &uuids);
  RegisterProperty(bluetooth_adapter::kModaliasProperty, &modalias);
}

ExperimentalBluetoothAdapterClient::Properties::~Properties() {
}


// The ExperimentalBluetoothAdapterClient implementation used in production.
class ExperimentalBluetoothAdapterClientImpl
    : public ExperimentalBluetoothAdapterClient,
      public dbus::ObjectManager::Interface {
 public:
  explicit ExperimentalBluetoothAdapterClientImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {
    object_manager_ = bus_->GetObjectManager(
        bluetooth_manager::kBluetoothManagerServiceName,
        dbus::ObjectPath(bluetooth_manager::kBluetoothManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface, this);
  }

  virtual ~ExperimentalBluetoothAdapterClientImpl() {
    object_manager_->UnregisterInterface(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface);
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual void AddObserver(
      ExperimentalBluetoothAdapterClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual void RemoveObserver(
      ExperimentalBluetoothAdapterClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // Returns the list of adapter object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetAdapters() OVERRIDE {
    return object_manager_->GetObjectsWithInterface(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface);
  }

  // dbus::ObjectManager::Interface override.
  virtual dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        interface_name,
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_manager_->GetProperties(
            object_path,
            bluetooth_adapter::kExperimentalBluetoothAdapterInterface));
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface,
        bluetooth_adapter::kStartDiscovery);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface,
        bluetooth_adapter::kStopDiscovery);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothAdapterClient override.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kExperimentalBluetoothAdapterInterface,
        bluetooth_adapter::kRemoveDevice);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(device_path);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownAdapterError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 private:
  // Called by dbus::ObjectManager when an object with the adapter interface
  // is created. Informs observers.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path,
                           const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothAdapterClient::Observer,
                      observers_,
                      AdapterAdded(object_path));
  }

  // Called by dbus::ObjectManager when an object with the adapter interface
  // is removed. Informs observers.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path,
                             const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothAdapterClient::Observer,
                      observers_,
                      AdapterRemoved(object_path));
  }

  // Called by dbus::PropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(ExperimentalBluetoothAdapterClient::Observer, observers_,
                      AdapterPropertyChanged(object_path, property_name));
  }

  // Called when a response for successful method call is received.
  void OnSuccess(const base::Closure& callback,
                 dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
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

  dbus::Bus* bus_;
  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  ObserverList<ExperimentalBluetoothAdapterClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ExperimentalBluetoothAdapterClientImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothAdapterClientImpl);
};

ExperimentalBluetoothAdapterClient::ExperimentalBluetoothAdapterClient() {
}

ExperimentalBluetoothAdapterClient::~ExperimentalBluetoothAdapterClient() {
}

ExperimentalBluetoothAdapterClient* ExperimentalBluetoothAdapterClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ExperimentalBluetoothAdapterClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeBluetoothAdapterClient();
}

}  // namespace chromeos

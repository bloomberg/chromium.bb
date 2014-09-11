// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_adapter_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char BluetoothAdapterClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char BluetoothAdapterClient::kUnknownAdapterError[] =
    "org.chromium.Error.UnknownAdapter";

BluetoothAdapterClient::Properties::Properties(
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

BluetoothAdapterClient::Properties::~Properties() {
}


// The BluetoothAdapterClient implementation used in production.
class BluetoothAdapterClientImpl
    : public BluetoothAdapterClient,
      public dbus::ObjectManager::Interface {
 public:
  BluetoothAdapterClientImpl()
      : object_manager_(NULL), weak_ptr_factory_(this) {}

  virtual ~BluetoothAdapterClientImpl() {
    object_manager_->UnregisterInterface(
        bluetooth_adapter::kBluetoothAdapterInterface);
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

  // Returns the list of adapter object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetAdapters() OVERRIDE {
    return object_manager_->GetObjectsWithInterface(
        bluetooth_adapter::kBluetoothAdapterInterface);
  }

  // dbus::ObjectManager::Interface override.
  virtual dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        interface_name,
        base::Bind(&BluetoothAdapterClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // BluetoothAdapterClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_manager_->GetProperties(
            object_path,
            bluetooth_adapter::kBluetoothAdapterInterface));
  }

  // BluetoothAdapterClient override.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
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
        base::Bind(&BluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothAdapterClient override.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
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
        base::Bind(&BluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothAdapterClient override.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_adapter::kBluetoothAdapterInterface,
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
        base::Bind(&BluetoothAdapterClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothAdapterClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    object_manager_ = bus->GetObjectManager(
        bluetooth_object_manager::kBluetoothObjectManagerServiceName,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_adapter::kBluetoothAdapterInterface, this);
  }

 private:
  // Called by dbus::ObjectManager when an object with the adapter interface
  // is created. Informs observers.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path,
                           const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterAdded(object_path));
  }

  // Called by dbus::ObjectManager when an object with the adapter interface
  // is removed. Informs observers.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path,
                             const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterRemoved(object_path));
  }

  // Called by dbus::PropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
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

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapterClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterClientImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterClientImpl);
};

BluetoothAdapterClient::BluetoothAdapterClient() {
}

BluetoothAdapterClient::~BluetoothAdapterClient() {
}

BluetoothAdapterClient* BluetoothAdapterClient::Create() {
  return new BluetoothAdapterClientImpl;
}

}  // namespace chromeos

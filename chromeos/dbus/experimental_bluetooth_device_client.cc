// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/experimental_bluetooth_device_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char ExperimentalBluetoothDeviceClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";
const char ExperimentalBluetoothDeviceClient::kUnknownDeviceError[] =
    "org.chromium.Error.UnknownDevice";

ExperimentalBluetoothDeviceClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_device::kAddressProperty, &address);
  RegisterProperty(bluetooth_device::kNameProperty, &name);
  RegisterProperty(bluetooth_device::kIconProperty, &icon);
  RegisterProperty(bluetooth_device::kClassProperty, &bluetooth_class);
  RegisterProperty(bluetooth_device::kAppearanceProperty, &appearance);
  RegisterProperty(bluetooth_device::kUUIDsProperty, &uuids);
  RegisterProperty(bluetooth_device::kPairedProperty, &paired);
  RegisterProperty(bluetooth_device::kConnectedProperty, &connected);
  RegisterProperty(bluetooth_device::kTrustedProperty, &trusted);
  RegisterProperty(bluetooth_device::kBlockedProperty, &blocked);
  RegisterProperty(bluetooth_device::kAliasProperty, &alias);
  RegisterProperty(bluetooth_device::kAdapterProperty, &adapter);
  RegisterProperty(bluetooth_device::kLegacyPairingProperty, &legacy_pairing);
  RegisterProperty(bluetooth_device::kModaliasProperty, &modalias);
  RegisterProperty(bluetooth_device::kRSSIProperty, &rssi);
}

ExperimentalBluetoothDeviceClient::Properties::~Properties() {
}


// The ExperimentalBluetoothDeviceClient implementation used in production.
class ExperimentalBluetoothDeviceClientImpl
    : public ExperimentalBluetoothDeviceClient,
      public dbus::ObjectManager::Interface {
 public:
  explicit ExperimentalBluetoothDeviceClientImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {
    object_manager_ = bus_->GetObjectManager(
        bluetooth_manager::kBluetoothManagerServiceName,
        dbus::ObjectPath(bluetooth_manager::kBluetoothManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_device::kExperimentalBluetoothDeviceInterface, this);
  }

  virtual ~ExperimentalBluetoothDeviceClientImpl() {
    object_manager_->UnregisterInterface(
        bluetooth_device::kExperimentalBluetoothDeviceInterface);
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void AddObserver(
      ExperimentalBluetoothDeviceClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void RemoveObserver(
      ExperimentalBluetoothDeviceClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // dbus::ObjectManager::Interface override.
  virtual dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        interface_name,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) OVERRIDE {
    std::vector<dbus::ObjectPath> object_paths, device_paths;
    device_paths = object_manager_->GetObjectsWithInterface(
        bluetooth_device::kExperimentalBluetoothDeviceInterface);
    for (std::vector<dbus::ObjectPath>::iterator iter = device_paths.begin();
         iter != device_paths.end(); ++iter) {
      Properties* properties = GetProperties(*iter);
      if (properties->adapter.value() == adapter_path)
        object_paths.push_back(*iter);
    }
    return object_paths;
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_manager_->GetProperties(
            object_path,
            bluetooth_device::kExperimentalBluetoothDeviceInterface));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kConnect);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }

    // Connect may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kDisconnect);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void ConnectProfile(const dbus::ObjectPath& object_path,
                              const std::string& uuid,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kConnectProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(uuid);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }

    // Connect may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void DisconnectProfile(const dbus::ObjectPath& object_path,
                                 const std::string& uuid,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback)
        OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kDisconnectProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(uuid);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void Pair(const dbus::ObjectPath& object_path,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kPair);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }

    // Pairing may take an arbitrary length of time, so use no timeout.
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothDeviceClient override.
  virtual void CancelPairing(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback)
        OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_device::kExperimentalBluetoothDeviceInterface,
        bluetooth_device::kCancelPairing);

    dbus::ObjectProxy* object_proxy =
        object_manager_->GetObjectProxy(object_path);
    if (!object_proxy) {
      error_callback.Run(kUnknownDeviceError, "");
      return;
    }
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothDeviceClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 private:
  // Called by dbus::ObjectManager when an object with the device interface
  // is created. Informs observers.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path,
                           const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer,
                      observers_,
                      DeviceAdded(object_path));
  }

  // Called by dbus::ObjectManager when an object with the device interface
  // is removed. Informs observers.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path,
                             const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer,
                      observers_,
                      DeviceRemoved(object_path));
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                      DevicePropertyChanged(object_path, property_name));
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
  ObserverList<ExperimentalBluetoothDeviceClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ExperimentalBluetoothDeviceClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothDeviceClientImpl);
};

ExperimentalBluetoothDeviceClient::ExperimentalBluetoothDeviceClient() {
}

ExperimentalBluetoothDeviceClient::~ExperimentalBluetoothDeviceClient() {
}

ExperimentalBluetoothDeviceClient* ExperimentalBluetoothDeviceClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ExperimentalBluetoothDeviceClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeBluetoothDeviceClient();
}

}  // namespace chromeos

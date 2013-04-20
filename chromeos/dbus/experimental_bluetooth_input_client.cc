// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/experimental_bluetooth_input_client.h"

#include <map>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ExperimentalBluetoothInputClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_input::kReconnectModeProperty, &reconnect_mode);
}

ExperimentalBluetoothInputClient::Properties::~Properties() {
}


// The ExperimentalBluetoothInputClient implementation used in production.
class ExperimentalBluetoothInputClientImpl
    : public ExperimentalBluetoothInputClient,
      public dbus::ObjectManager::Interface {
 public:
  explicit ExperimentalBluetoothInputClientImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {
    object_manager_ = bus_->GetObjectManager(
        bluetooth_manager::kBluetoothManagerServiceName,
        dbus::ObjectPath(bluetooth_manager::kBluetoothManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_input::kExperimentalBluetoothInputInterface, this);
  }

  virtual ~ExperimentalBluetoothInputClientImpl() {
    object_manager_->UnregisterInterface(
        bluetooth_input::kExperimentalBluetoothInputInterface);
  }

  // ExperimentalBluetoothInputClient override.
  virtual void AddObserver(
      ExperimentalBluetoothInputClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // ExperimentalBluetoothInputClient override.
  virtual void RemoveObserver(
      ExperimentalBluetoothInputClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // dbus::ObjectManager::Interface override.
  virtual dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) {
    Properties* properties = new Properties(
        object_proxy, interface_name,
        base::Bind(&ExperimentalBluetoothInputClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // ExperimentalBluetoothInputClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_manager_->GetProperties(
            object_path,
            bluetooth_input::kExperimentalBluetoothInputInterface));
  }

 private:
  // Called by dbus::ObjectManager when an object with the input interface
  // is created. Informs observers.
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothInputClient::Observer, observers_,
                      InputAdded(object_path));
  }

  // Called by dbus::ObjectManager when an object with the input interface
  // is removed. Informs observers.
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) OVERRIDE {
    FOR_EACH_OBSERVER(ExperimentalBluetoothInputClient::Observer, observers_,
                      InputRemoved(object_path));
  }

  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    FOR_EACH_OBSERVER(ExperimentalBluetoothInputClient::Observer, observers_,
                      InputPropertyChanged(object_path, property_name));
  }

  dbus::Bus* bus_;
  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  ObserverList<ExperimentalBluetoothInputClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ExperimentalBluetoothInputClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothInputClientImpl);
};

ExperimentalBluetoothInputClient::ExperimentalBluetoothInputClient() {
}

ExperimentalBluetoothInputClient::~ExperimentalBluetoothInputClient() {
}

ExperimentalBluetoothInputClient* ExperimentalBluetoothInputClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ExperimentalBluetoothInputClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeBluetoothInputClient();
}

}  // namespace chromeos

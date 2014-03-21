// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/object_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

BluetoothGattCharacteristicClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(bluetooth_gatt_characteristic::kUUIDProperty, &uuid);
  RegisterProperty(bluetooth_gatt_characteristic::kServiceProperty, &service);
  RegisterProperty(bluetooth_gatt_characteristic::kValueProperty, &value);
  RegisterProperty(bluetooth_gatt_characteristic::kFlagsProperty, &flags);
}

BluetoothGattCharacteristicClient::Properties::~Properties() {
}

// The BluetoothGattCharacteristicClient implementation used in production.
class BluetoothGattCharacteristicClientImpl
    : public BluetoothGattCharacteristicClient,
      public dbus::ObjectManager::Interface {
 public:
  BluetoothGattCharacteristicClientImpl()
      : object_manager_(NULL),
        weak_ptr_factory_(this) {
  }

  virtual ~BluetoothGattCharacteristicClientImpl() {
    object_manager_->UnregisterInterface(
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);
  }

  // BluetoothGattCharacteristicClient override.
  virtual void AddObserver(
      BluetoothGattCharacteristicClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothGattCharacteristicClient override.
  virtual void RemoveObserver(
      BluetoothGattCharacteristicClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothGattCharacteristicClient override.
  virtual std::vector<dbus::ObjectPath> GetCharacteristics() OVERRIDE {
    DCHECK(object_manager_);
    return object_manager_->GetObjectsWithInterface(
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);
  }

  // BluetoothGattCharacteristicClient override.
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE {
    DCHECK(object_manager_);
    return static_cast<Properties*>(
        object_manager_->GetProperties(
            object_path,
            bluetooth_gatt_characteristic::
                kBluetoothGattCharacteristicInterface));
  }

  // dbus::ObjectManager::Interface override.
  virtual dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy *object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        interface_name,
        base::Bind(&BluetoothGattCharacteristicClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_path));
    return static_cast<dbus::PropertySet*>(properties);
  }

  // dbus::ObjectManager::Interface override.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path,
                           const std::string& interface_name) OVERRIDE {
    VLOG(2) << "Remote GATT characteristic added: " << object_path.value();
    FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                      GattCharacteristicAdded(object_path));
  }

  // dbus::ObjectManager::Interface override.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path,
                             const std::string& interface_name) OVERRIDE {
    VLOG(2) << "Remote GATT characteristic removed: " << object_path.value();
    FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                      GattCharacteristicRemoved(object_path));
  }

 protected:
  // chromeos::DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    object_manager_ = bus->GetObjectManager(
        bluetooth_object_manager::kBluetoothObjectManagerServiceName,
        dbus::ObjectPath(
            bluetooth_object_manager::kBluetoothObjectManagerServicePath));
    object_manager_->RegisterInterface(
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
        this);
  }

 private:
  // Called by dbus::PropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call. Informs
  // observers.
  virtual void OnPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name) {
    VLOG(2) << "Remote GATT characteristic property changed: "
            << object_path.value() << ": " << property_name;
    FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                      GattCharacteristicPropertyChanged(object_path,
                                                        property_name));
  }

  dbus::ObjectManager* object_manager_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothGattCharacteristicClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattCharacteristicClientImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattCharacteristicClientImpl);
};

BluetoothGattCharacteristicClient::BluetoothGattCharacteristicClient() {
}

BluetoothGattCharacteristicClient::~BluetoothGattCharacteristicClient() {
}

// static
BluetoothGattCharacteristicClient* BluetoothGattCharacteristicClient::Create() {
  return new BluetoothGattCharacteristicClientImpl();
}

}  // namespace chromeos

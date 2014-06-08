// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeBluetoothGattDescriptorClient simulates the behavior of the Bluetooth
// Daemon GATT characteristic descriptor objects and is used in test cases in
// place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattDescriptorClient
    : public BluetoothGattDescriptorClient {
 public:
  struct Properties : public BluetoothGattDescriptorClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothGattDescriptorClient();
  virtual ~FakeBluetoothGattDescriptorClient();

  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE;

  // BluetoothGattDescriptorClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetDescriptors() OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;
  virtual void ReadValue(const dbus::ObjectPath& object_path,
                         const ValueCallback& callback,
                         const ErrorCallback& error_callback) OVERRIDE;
  virtual void WriteValue(const dbus::ObjectPath& object_path,
                          const std::vector<uint8>& value,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;

  // Makes the descriptor with the UUID |uuid| visible under the characteristic
  // with object path |characteristic_path|. Descriptor object paths are
  // hierarchical to their characteristics. |uuid| must belong to a descriptor
  // for which there is a constant defined below, otherwise this method has no
  // effect. Returns the object path of the created descriptor. In the no-op
  // case, returns an invalid path.
  dbus::ObjectPath ExposeDescriptor(const dbus::ObjectPath& characteristic_path,
                                    const std::string& uuid);
  void HideDescriptor(const dbus::ObjectPath& descriptor_path);

  // Object path components and UUIDs of GATT characteristic descriptors.
  static const char kClientCharacteristicConfigurationPathComponent[];
  static const char kClientCharacteristicConfigurationUUID[];

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Notifies observers.
  void NotifyDescriptorAdded(const dbus::ObjectPath& object_path);
  void NotifyDescriptorRemoved(const dbus::ObjectPath& object_path);

  // Mapping from object paths to Properties structures.
  struct DescriptorData {
    DescriptorData();
    ~DescriptorData();

    scoped_ptr<Properties> properties;
    std::vector<uint8> value;
  };
  typedef std::map<dbus::ObjectPath, DescriptorData*> PropertiesMap;
  PropertiesMap properties_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeBluetoothGattDescriptorClient>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattDescriptorClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

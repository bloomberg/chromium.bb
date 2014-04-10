// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothGattServiceClient simulates the behavior of the Bluetooth Daemon
// GATT service objects and is used in test cases in place of a mock and on the
// Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattServiceClient
    : public BluetoothGattServiceClient {
 public:
  struct Properties : public BluetoothGattServiceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothGattServiceClient();
  virtual ~FakeBluetoothGattServiceClient();

  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE;

  // BluetoothGattServiceClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetServices() OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;

  // Makes a service visible for device with object path |device_path|. Note
  // that only one instance of a specific service is simulated at a time. Hence,
  // this method will fail, if the service is already visible.
  void ExposeHeartRateService(const dbus::ObjectPath& device_path);
  void HideHeartRateService();

  // Returns whether or not the Heart Rate Service is visible.
  bool IsHeartRateVisible() const;

  // Returns the current object path of the visible Heart Rate service. If the
  // service is not visible, returns an invalid empty path.
  dbus::ObjectPath GetHeartRateServicePath() const;

  // Final object path components and the corresponding UUIDs of the GATT
  // services that we emulate. Service paths are hierarchical to Bluetooth
  // device paths, so if the path component is "service0000", and the device
  // path is "/org/foo/device0", the service path is
  // "/org/foo/device0/service0000".
  static const char kHeartRateServicePathComponent[];
  static const char kHeartRateServiceUUID[];

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Notifies observers.
  void NotifyServiceAdded(const dbus::ObjectPath& object_path);
  void NotifyServiceRemoved(const dbus::ObjectPath& object_path);

  // Tells FakeBluetoothGattCharacteristicClient to expose GATT characteristics.
  // This is scheduled from ExposeHeartRateService to simulate asynchronous
  // retrieval of characteristics. If the Heart Rate Service is hidden at the
  // time this method is called, then it does nothing.
  void ExposeHeartRateCharacteristics();

  // Static properties we return. As long as a service is exposed, this will be
  // non-null. Otherwise it will be null.
  scoped_ptr<Properties> heart_rate_service_properties_;
  std::string heart_rate_service_path_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeBluetoothGattServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_CLIENT_H_

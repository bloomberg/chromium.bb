// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothGattCharacteristicClient simulates the behavior of the
// Bluetooth Daemon GATT characteristic objects and is used in test cases in
// place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattCharacteristicClient
    : public BluetoothGattCharacteristicClient {
 public:
  struct Properties : public BluetoothGattCharacteristicClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothGattCharacteristicClient();
  virtual ~FakeBluetoothGattCharacteristicClient();

  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE;

  // BluetoothGattCharacteristicClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetCharacteristics() OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;
  virtual void ReadValue(const dbus::ObjectPath& object_path,
                         const ValueCallback& callback,
                         const ErrorCallback& error_callback) OVERRIDE;
  virtual void WriteValue(const dbus::ObjectPath& object_path,
                          const std::vector<uint8>& value,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void StartNotify(const dbus::ObjectPath& object_path,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void StopNotify(const dbus::ObjectPath& object_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;

  // Makes the group of characteristics belonging to a particular GATT based
  // profile available under the GATT service with object path |service_path|.
  // Characteristic paths are hierarchical to service paths.
  void ExposeHeartRateCharacteristics(const dbus::ObjectPath& service_path);
  void HideHeartRateCharacteristics();

  // Returns whether or not the heart rate characteristics are visible and
  // performs the appropriate assertions.
  bool IsHeartRateVisible() const;

  // Returns the current object paths of exposed characteristics. If the
  // characteristic is not visible, returns an invalid empty path.
  dbus::ObjectPath GetHeartRateMeasurementPath() const;
  dbus::ObjectPath GetBodySensorLocationPath() const;
  dbus::ObjectPath GetHeartRateControlPointPath() const;

  // Object path components and UUIDs of GATT characteristics.
  // Heart Rate Service:
  static const char kHeartRateMeasurementPathComponent[];
  static const char kHeartRateMeasurementUUID[];
  static const char kBodySensorLocationPathComponent[];
  static const char kBodySensorLocationUUID[];
  static const char kHeartRateControlPointPathComponent[];
  static const char kHeartRateControlPointUUID[];

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Notifies observers.
  void NotifyCharacteristicAdded(const dbus::ObjectPath& object_path);
  void NotifyCharacteristicRemoved(const dbus::ObjectPath& object_path);

  // Schedules a heart rate measurement value change, if the heart rate
  // characteristics are visible.
  void ScheduleHeartRateMeasurementValueChange();

  // Returns a random Heart Rate Measurement value. All the fields of the value
  // are populated according to the the fake behavior. The measurement value
  // is a random value within a reasonable range.
  std::vector<uint8> GetHeartRateMeasurementValue();

  // If true, characteristics of the Heart Rate Service are visible. Use
  // IsHeartRateVisible() to check the value.
  bool heart_rate_visible_;

  // Total calories burned, used for the Heart Rate Measurement characteristic.
  uint16 calories_burned_;

  // Static properties returned for simulated characteristics for the Heart
  // Rate Service. These pointers are not NULL only if the characteristics are
  // actually exposed.
  scoped_ptr<Properties> heart_rate_measurement_properties_;
  scoped_ptr<Properties> body_sensor_location_properties_;
  scoped_ptr<Properties> heart_rate_control_point_properties_;

  // Object paths of the exposed characteristics. If a characteristic is not
  // exposed, these will be empty.
  std::string heart_rate_measurement_path_;
  std::string heart_rate_measurement_ccc_desc_path_;
  std::string body_sensor_location_path_;
  std::string heart_rate_control_point_path_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeBluetoothGattCharacteristicClient>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattCharacteristicClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_

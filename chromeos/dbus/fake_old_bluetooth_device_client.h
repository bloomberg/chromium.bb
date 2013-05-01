// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_DEVICE_CLIENT_H_

#include "chromeos/dbus/bluetooth_device_client.h"

namespace chromeos {

// A fake implementation of BluetoothDeviceClient used on Linux desktop and for
// tests. . This class does nothing.
class FakeOldBluetoothDeviceClient : public BluetoothDeviceClient {
 public:
  struct Properties : public BluetoothDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);

    virtual ~Properties();

    // BluetoothDeviceClient::Properties overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase *property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeOldBluetoothDeviceClient();
  virtual ~FakeOldBluetoothDeviceClient();

  // BluetoothDeviceClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DiscoverServices(const dbus::ObjectPath& object_path,
                                const std::string& pattern,
                                const ServicesCallback& callback) OVERRIDE;
  virtual void CancelDiscovery(const dbus::ObjectPath& object_path,
                               const DeviceCallback& callback) OVERRIDE;
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const DeviceCallback& callback) OVERRIDE;
  virtual void CreateNode(const dbus::ObjectPath& object_path,
                          const std::string& uuid,
                          const NodeCallback& callback) OVERRIDE;
  virtual void RemoveNode(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& node_path,
                          const DeviceCallback& callback) OVERRIDE;

 private:
  void OnPropertyChanged(dbus::ObjectPath object_path,
                         const std::string& property_name);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we typedef.
  typedef std::map<const dbus::ObjectPath, Properties *> PropertiesMap;
  PropertiesMap properties_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeOldBluetoothDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_DEVICE_CLIENT_H_

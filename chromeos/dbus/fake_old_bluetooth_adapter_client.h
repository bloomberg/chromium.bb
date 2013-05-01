// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_ADAPTER_CLIENT_H_

#include "chromeos/dbus/bluetooth_adapter_client.h"

namespace chromeos {

// A fake implementation of BluetoothAdapterClient used on Linux desktop and for
// tests. This class does nothing.
class FakeOldBluetoothAdapterClient : public BluetoothAdapterClient {
 public:
  struct Properties : public BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // BluetoothAdapterClient::Properties overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase *property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeOldBluetoothAdapterClient();
  virtual ~FakeOldBluetoothAdapterClient();

  // BluetoothAdapterClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RequestSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE;
  virtual void ReleaseSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE;
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) OVERRIDE;
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const AdapterCallback& callback) OVERRIDE;
  virtual void FindDevice(const dbus::ObjectPath& object_path,
                          const std::string& address,
                          const DeviceCallback& callback) OVERRIDE;
  virtual void CreateDevice(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) OVERRIDE;
  virtual void CreatePairedDevice(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const dbus::ObjectPath& agent_path,
      const std::string& capability,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) OVERRIDE;
  virtual void CancelDeviceCreation(const dbus::ObjectPath& object_path,
                                    const std::string& address,
                                    const AdapterCallback& callback) OVERRIDE;
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const AdapterCallback& callback) OVERRIDE;
  virtual void RegisterAgent(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& agent_path,
                             const std::string& capability,
                             const AdapterCallback& callback) OVERRIDE;
  virtual void UnregisterAgent(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& agent_path,
                               const AdapterCallback& callback) OVERRIDE;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  scoped_ptr<Properties> properties_;

 private:
  void OnPropertyChanged(const std::string& property_name);

  DISALLOW_COPY_AND_ASSIGN(FakeOldBluetoothAdapterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_ADAPTER_CLIENT_H_

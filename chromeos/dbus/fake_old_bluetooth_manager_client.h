// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_MANAGER_CLIENT_H_

#include "chromeos/dbus/bluetooth_manager_client.h"

namespace chromeos {

// A fake implementation of BluetoothManagerClient used on Linux desktop and for
// tests. This class does nothing.
class FakeOldBluetoothManagerClient : public BluetoothManagerClient {
 public:
  struct Properties : public BluetoothManagerClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // BluetoothManagerClient::Properties overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeOldBluetoothManagerClient();
  virtual ~FakeOldBluetoothManagerClient();

  // BluetoothManagerClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties() OVERRIDE;
  virtual void DefaultAdapter(const AdapterCallback& callback) OVERRIDE;
  virtual void FindAdapter(const std::string& address,
                           const AdapterCallback& callback) OVERRIDE;

 private:
  void OnPropertyChanged(const std::string& property_name);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  scoped_ptr<Properties> properties_;

  DISALLOW_COPY_AND_ASSIGN(FakeOldBluetoothManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_OLD_BLUETOOTH_MANAGER_CLIENT_H_

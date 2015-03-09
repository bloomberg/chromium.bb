// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_AP_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_AP_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/dbus/ap_manager_client.h"

namespace chromeos {

// A fake implementation of ApManagerClient. Invokes callbacks
// immediately.
class FakeApManagerClient : public ApManagerClient {
 public:
  FakeApManagerClient();
  ~FakeApManagerClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // ApManagerClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void CreateService(const ObjectPathDBusMethodCallback& callback) override;
  void RemoveService(const dbus::ObjectPath& object_path,
                     const VoidDBusMethodCallback& callback) override;
  void StartService(const dbus::ObjectPath& object_path,
                    const VoidDBusMethodCallback& callback) override;
  void StopService(const dbus::ObjectPath& object_path,
                   const VoidDBusMethodCallback& callback) override;
  ConfigProperties* GetConfigProperties(
      const dbus::ObjectPath& object_path) override;
  const DeviceProperties* GetDeviceProperties(
      const dbus::ObjectPath& object_path) override;
  const ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeApManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_AP_MANAGER_CLIENT_H_

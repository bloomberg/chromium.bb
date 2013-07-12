// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_

#include "base/callback_forward.h"
#include "base/values.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"

namespace chromeos {

// A fake implementation of ShillDeviceClient. This class does nothing.
class FakeShillDeviceClient : public ShillDeviceClient {
 public:
  FakeShillDeviceClient();
  virtual ~FakeShillDeviceClient();

  // ShillDeviceClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) OVERRIDE;
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE;
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void Reset(const dbus::ObjectPath& device_path,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) OVERRIDE;
  virtual TestInterface* GetTestInterface() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_

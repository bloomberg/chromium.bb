// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_DEVICE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/shill_client_helper.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class Bus;
class ObjectPath;

}  // namespace dbus

namespace chromeos {

class ShillPropertyChangedObserver;

// ShillDeviceClient is used to communicate with the Shill Device service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class CHROMEOS_EXPORT ShillDeviceClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up devices for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void AddDevice(const std::string& device_path,
                           const std::string& type,
                           const std::string& object_path,
                           const std::string& connection_path) = 0;
    virtual void RemoveDevice(const std::string& device_path) = 0;
    virtual void ClearDevices() = 0;

   protected:
    ~TestInterface() {}
  };

  virtual ~ShillDeviceClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillDeviceClient* Create(DBusClientImplementationType type,
                                      dbus::Bus* bus);

  // Adds a property changed |observer| for the device at |device_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the device at |device_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call finishes.
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) = 0;

  // DEPRECATED DO NOT USE: Calls GetProperties method and blocks until the
  // method call finishes.  The caller is responsible to delete the result.
  // Thie method returns NULL when method call fails.
  //
  // TODO(hashimoto): Refactor CrosGetDeviceNetworkList and remove this method.
  // crosbug.com/29902
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) = 0;

  // Calls ProposeScan method.
  // |callback| is called after the method call finishes.
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call finishes.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call finishes.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) = 0;

  // Calls AddIPConfig method.
  // |callback| is called after the method call finishes.
  virtual void AddIPConfig(const dbus::ObjectPath& device_path,
                           const std::string& method,
                           const ObjectPathDBusMethodCallback& callback) = 0;

  // Calls the RequirePin method.
  // |callback| is called after the method call finishes.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls the EnterPin method.
  // |callback| is called after the method call finishes.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Calls the UnblockPin method.
  // |callback| is called after the method call finishes.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls the ChangePin method.
  // |callback| is called after the method call finishes.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) = 0;

  // Calls the Register method.
  // |callback| is called after the method call finishes.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Calls the SetCarrier method.
  // |callback| is called after the method call finishes.
  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Returns an interface for testing (stub only), or returns NULL.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Create() should be used instead.
  ShillDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_DEVICE_CLIENT_H_

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

// ShillDeviceClient is used to communicate with the Shill Device service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class CHROMEOS_EXPORT ShillDeviceClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  virtual ~ShillDeviceClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillDeviceClient* Create(DBusClientImplementationType type,
                                      dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& device_path,
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& device_path) = 0;

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
                           const VoidDBusMethodCallback& callback) = 0;

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

  // DEPRECATED DO NOT USE: Calls AddIPConfig method and blocks until the method
  // call finishes.
  // This method returns an empty path when method call fails.
  //
  // TODO(hashimoto): Refactor CrosAddIPConfig and remove this method.
  // crosbug.com/29902
  virtual dbus::ObjectPath CallAddIPConfigAndBlock(
      const dbus::ObjectPath& device_path,
      const std::string& method) = 0;

  // Calls RequirePin method.
  // |callback| is called after the method call finishes.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls EnterPin method.
  // |callback| is called after the method call finishes.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Calls UnblockPin method.
  // |callback| is called after the method call finishes.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls ChangePin method.
  // |callback| is called after the method call finishes.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) = 0;

  // Calls Register method.
  // |callback| is called after the method call finishes.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

 protected:
  // Create() should be used instead.
  ShillDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_DEVICE_CLIENT_H_

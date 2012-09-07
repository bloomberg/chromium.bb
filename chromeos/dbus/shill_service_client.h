// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_H_

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

// ShillServiceClient is used to communicate with the Shill Service
// service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class CHROMEOS_EXPORT ShillServiceClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  virtual ~ShillServiceClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillServiceClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& service_path,
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& service_path) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) = 0;

  // Calls Connect method.
  // |callback| is called after the method call succeeds.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  // Calls Disconnect method.
  // |callback| is called after the method call succeeds.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const VoidDBusMethodCallback& callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const VoidDBusMethodCallback& callback) = 0;

  // Calls ActivateCellularModem method.
  // |callback| is called after the method call succeeds.
  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const VoidDBusMethodCallback& callback) = 0;

  // DEPRECATED DO NOT USE: Calls ActivateCellularModem method and blocks until
  // the method call finishes.
  //
  // TODO(hashimoto): Refactor CrosActivateCellularModem and remove this method.
  // crosbug.com/29902
  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) = 0;

 protected:
  // Create() should be used instead.
  ShillServiceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_H_

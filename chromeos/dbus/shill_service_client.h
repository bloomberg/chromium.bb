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
  typedef ShillClientHelper::ListValueCallback ListValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up services for testing. Accessed through
  // GetTestInterface(), only implemented in the stub implementation.
  class TestInterface {
   public:
    virtual void AddService(const std::string& service_path,
                            const std::string& name,
                            const std::string& type,
                            const std::string& state,
                            bool add_to_watch_list) = 0;
    virtual void RemoveService(const std::string& service_path) = 0;
    virtual void SetServiceProperty(const std::string& service_path,
                                    const std::string& property,
                                    const base::Value& value) = 0;
    virtual const base::DictionaryValue* GetServiceProperties(
        const std::string& service_path) const = 0;
    virtual void ClearServices() = 0;

   protected:
    ~TestInterface() {}
  };
  virtual ~ShillServiceClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillServiceClient* Create(DBusClientImplementationType type,
                                    dbus::Bus* bus);

  // Adds a property changed |observer| to the service at |service_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| to the service at |service_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

  // Calls ClearProperties method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               const ListValueCallback& callback,
                               const ErrorCallback& error_callback) = 0;

  // Calls Connect method.
  // |callback| is called after the method call succeeds.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  // Calls Disconnect method.
  // |callback| is called after the method call succeeds.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) = 0;

  // Calls ActivateCellularModem method.
  // |callback| is called after the method call succeeds.
  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Calls the CompleteCellularActivation method.
  // |callback| is called after the method call succeeds.
  virtual void CompleteCellularActivation(
      const dbus::ObjectPath& service_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // DEPRECATED DO NOT USE: Calls ActivateCellularModem method and blocks until
  // the method call finishes.
  //
  // TODO(hashimoto): Refactor CrosActivateCellularModem and remove this method.
  // crosbug.com/29902
  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) = 0;

  // Returns an interface for testing (stub only), or returns NULL.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Create() should be used instead.
  ShillServiceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/shill_client_helper.h"

namespace dbus {

class Bus;

}  // namespace dbus

namespace chromeos {

// ShillManagerClient is used to communicate with the Shill Manager
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT ShillManagerClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;

  virtual ~ShillManagerClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillManagerClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler() = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const DictionaryValueCallback& callback) = 0;

  // DEPRECATED DO NOT USE: Calls GetProperties method and blocks until the
  // method call finishes.  The caller is responsible to delete the result.
  // Thie method returns NULL when method call fails.
  //
  // TODO(hashimoto): Refactor CrosGetWifiAccessPoints and remove this method.
  // crosbug.com/29902
  virtual base::DictionaryValue* CallGetPropertiesAndBlock() = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) = 0;

  // Calls RequestScan method.
  // |callback| is called after the method call succeeds.
  virtual void RequestScan(const std::string& type,
                           const VoidDBusMethodCallback& callback) = 0;

  // Calls EnableTechnology method.
  // |callback| is called after the method call succeeds.
  virtual void EnableTechnology(const std::string& type,
                                const VoidDBusMethodCallback& callback) = 0;

  // Calls DisableTechnology method.
  // |callback| is called after the method call succeeds.
  virtual void DisableTechnology(const std::string& type,
                                 const VoidDBusMethodCallback& callback) = 0;

  // Calls ConfigureService method.
  // |callback| is called after the method call succeeds.
  virtual void ConfigureService(const base::DictionaryValue& properties,
                                const VoidDBusMethodCallback& callback) = 0;

  // Calls GetService method.
  // |callback| is called after the method call succeeds.
  virtual void GetService(const base::DictionaryValue& properties,
                          const ObjectPathDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  ShillManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_H_

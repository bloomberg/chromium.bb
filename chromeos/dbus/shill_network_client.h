// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_NETWORK_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_NETWORK_CLIENT_H_

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

// ShillNetworkClient is used to communicate with the Shill Network
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT ShillNetworkClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;

  virtual ~ShillNetworkClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillNetworkClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus);

  // Adds a property changed |observer| for the network at |network_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the network at |network_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& network_path,
                             const DictionaryValueCallback& callback) = 0;

  // DEPRECATED DO NOT USE: Calls GetProperties method and blocks until the
  // method call finishes.  The caller is responsible to delete the result.
  // Thie method returns NULL when method call fails.
  //
  // TODO(hashimoto): Refactor CrosGetWifiAccessPoints and remove this method.
  // crosbug.com/29902
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& network_path) = 0;

 protected:
  // Create() should be used instead.
  ShillNetworkClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillNetworkClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_NETWORK_CLIENT_H_

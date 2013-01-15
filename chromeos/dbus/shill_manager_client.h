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

class ShillPropertyChangedObserver;

// ShillManagerClient is used to communicate with the Shill Manager
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT ShillManagerClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up devices, services, and technologies for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void AddDevice(const std::string& device_path) = 0;
    virtual void RemoveDevice(const std::string& device_path) = 0;
    virtual void ResetDevices() = 0;
    virtual void AddService(const std::string& service_path,
                            bool add_to_watch_list) = 0;
    virtual void AddServiceAtIndex(const std::string& service_path,
                                   size_t index,
                                   bool add_to_watch_list) = 0;
    virtual void RemoveService(const std::string& service_path) = 0;
    virtual void AddTechnology(const std::string& type, bool enabled) = 0;
    virtual void RemoveTechnology(const std::string& type) = 0;
    virtual void AddGeoNetwork(const std::string& technology,
                               const base::DictionaryValue& network) = 0;

    // Used to reset all properties; does not notify observers.
    virtual void ClearProperties() = 0;

   protected:
    ~TestInterface() {}
  };

  virtual ~ShillManagerClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillManagerClient* Create(DBusClientImplementationType type,
                                    dbus::Bus* bus);

  // Adds a property changed |observer|.
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer|.
  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const DictionaryValueCallback& callback) = 0;

  // DEPRECATED DO NOT USE: Calls GetProperties method and blocks until the
  // method call finishes.  The caller is responsible to delete the result.
  // Thie method returns NULL when method call fails.
  //
  // TODO(hashimoto): Refactor blocking calls and remove this method.
  // crosbug.com/29902
  virtual base::DictionaryValue* CallGetPropertiesAndBlock() = 0;

  // Calls GetNetworksForGeolocation method.
  // |callback| is called after the method call succeeds.
  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls RequestScan method.
  // |callback| is called after the method call succeeds.
  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls EnableTechnology method.
  // |callback| is called after the method call succeeds.
  virtual void EnableTechnology(const std::string& type,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback) = 0;

  // Calls DisableTechnology method.
  // |callback| is called after the method call succeeds.
  virtual void DisableTechnology(const std::string& type,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Calls ConfigureService method.
  // |callback| is called after the method call succeeds.
  virtual void ConfigureService(const base::DictionaryValue& properties,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback) = 0;

  // Calls GetService method.
  // |callback| is called after the method call succeeds.
  virtual void GetService(const base::DictionaryValue& properties,
                          const ObjectPathCallback& callback,
                          const ErrorCallback& error_callback) = 0;

  // Returns an interface for testing (stub only), or returns NULL.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Create() should be used instead.
  ShillManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_STUB_H_
#define CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_STUB_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/shill_service_client.h"

namespace chromeos {

// A stub implementation of ShillServiceClient.
class ShillServiceClientStub : public ShillServiceClient,
                               public ShillServiceClient::TestInterface {
 public:
  ShillServiceClientStub();
  virtual ~ShillServiceClientStub();

  // ShillServiceClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               const ListValueCallback& callback,
                               const ErrorCallback& error_callback) OVERRIDE;
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void CompleteCellularActivation(
      const dbus::ObjectPath& service_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) OVERRIDE;
  virtual ShillServiceClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillServiceClient::TestInterface overrides.
  virtual void AddService(const std::string& service_path,
                          const std::string& name,
                          const std::string& type,
                          const std::string& state,
                          bool add_to_watch_list) OVERRIDE;
  virtual void AddServiceWithIPConfig(const std::string& service_path,
                                      const std::string& name,
                                      const std::string& type,
                                      const std::string& state,
                                      const std::string& ipconfig_path,
                                      bool add_to_watch_list) OVERRIDE;
  virtual void RemoveService(const std::string& service_path) OVERRIDE;
  virtual void SetServiceProperty(const std::string& service_path,
                                  const std::string& property,
                                  const base::Value& value) OVERRIDE;
  virtual const base::DictionaryValue* GetServiceProperties(
      const std::string& service_path) const OVERRIDE;
  virtual void ClearServices() OVERRIDE;

 private:
  typedef ObserverList<ShillPropertyChangedObserver> PropertyObserverList;

  void SetDefaultProperties();
  void PassStubServiceProperties(const dbus::ObjectPath& service_path,
                                 const DictionaryValueCallback& callback);
  void NotifyObserversPropertyChanged(const dbus::ObjectPath& service_path,
                                      const std::string& property);
  base::DictionaryValue* GetModifiableServiceProperties(
      const std::string& service_path);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);

  base::DictionaryValue stub_services_;
  // Observer list for each service.
  std::map<dbus::ObjectPath, PropertyObserverList*> observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillServiceClientStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillServiceClientStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SERVICE_CLIENT_STUB_H_

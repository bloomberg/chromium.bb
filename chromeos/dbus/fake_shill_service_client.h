// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_SERVICE_CLIENT_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_service_client.h"

namespace chromeos {

// A fake implementation of ShillServiceClient. This works in close coordination
// with FakeShillManagerClient and is not intended to be used independently.
class CHROMEOS_EXPORT FakeShillServiceClient
    : public ShillServiceClient,
      public ShillServiceClient::TestInterface {
 public:
  FakeShillServiceClient();
  virtual ~FakeShillServiceClient();

  // ShillServiceClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
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
  virtual void SetProperties(const dbus::ObjectPath& service_path,
                             const base::DictionaryValue& properties,
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
  virtual void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const DictionaryValueCallback& callback) OVERRIDE;
  virtual ShillServiceClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillServiceClient::TestInterface overrides.
  virtual void AddService(const std::string& service_path,
                          const std::string& guid,
                          const std::string& name,
                          const std::string& type,
                          const std::string& state,
                          bool visible) OVERRIDE;
  virtual void AddServiceWithIPConfig(const std::string& service_path,
                                      const std::string& guid,
                                      const std::string& name,
                                      const std::string& type,
                                      const std::string& state,
                                      const std::string& ipconfig_path,
                                      bool visible) OVERRIDE;
  virtual base::DictionaryValue* SetServiceProperties(
      const std::string& service_path,
      const std::string& guid,
      const std::string& name,
      const std::string& type,
      const std::string& state,
      bool visible) OVERRIDE;
  virtual void RemoveService(const std::string& service_path) OVERRIDE;
  virtual bool SetServiceProperty(const std::string& service_path,
                                  const std::string& property,
                                  const base::Value& value) OVERRIDE;
  virtual const base::DictionaryValue* GetServiceProperties(
      const std::string& service_path) const OVERRIDE;
  virtual void ClearServices() OVERRIDE;
  virtual void SetConnectBehavior(const std::string& service_path,
                                  const base::Closure& behavior) OVERRIDE;

 private:
  typedef ObserverList<ShillPropertyChangedObserver> PropertyObserverList;

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& service_path,
                                      const std::string& property);
  base::DictionaryValue* GetModifiableServiceProperties(
      const std::string& service_path,
      bool create_if_missing);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);
  void SetOtherServicesOffline(const std::string& service_path);
  void SetCellularActivated(const dbus::ObjectPath& service_path,
                            const ErrorCallback& error_callback);
  void ContinueConnect(const std::string& service_path);

  base::DictionaryValue stub_services_;

  // Per network service, stores a closure that is executed on each connection
  // attempt. The callback can for example modify the services properties in
  // order to simulate a connection failure.
  std::map<std::string, base::Closure> connect_behavior_;

  // Observer list for each service.
  std::map<dbus::ObjectPath, PropertyObserverList*> observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_SERVICE_CLIENT_H_

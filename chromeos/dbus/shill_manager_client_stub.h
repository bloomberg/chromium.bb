// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_STUB_H_
#define CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/dbus/shill_manager_client.h"

namespace chromeos {

// A stub implementation of ShillManagerClient.
// Implemented: Stub devices and services for NetworkStateManager tests.
// Implemented: Stub cellular device entry for SMS tests.
class ShillManagerClientStub : public ShillManagerClient,
                               public ShillManagerClient::TestInterface {
 public:
  ShillManagerClientStub();
  virtual ~ShillManagerClientStub();

  // ShillManagerClient overrides.
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE;
  virtual base::DictionaryValue* CallGetPropertiesAndBlock() OVERRIDE;
  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) OVERRIDE;
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void EnableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void DisableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConfigureService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void GetService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyDestination(const std::string& certificate,
                                 const std::string& public_key,
                                 const std::string& nonce,
                                 const std::string& signed_data,
                                 const std::string& device_serial,
                                 const BooleanCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptCredentials(
      const std::string& certificate,
      const std::string& public_key,
      const std::string& nonce,
      const std::string& signed_data,
      const std::string& device_serial,
      const std::string& service_path,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptData(
      const std::string& certificate,
      const std::string& public_key,
      const std::string& nonce,
      const std::string& signed_data,
      const std::string& device_serial,
      const std::string& data,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectToBestServices(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual ShillManagerClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillManagerClient::TestInterface overrides.

  virtual void AddDevice(const std::string& device_path) OVERRIDE;
  virtual void RemoveDevice(const std::string& device_path) OVERRIDE;
  virtual void ClearDevices() OVERRIDE;
  virtual void ClearServices() OVERRIDE;
  virtual void AddService(const std::string& service_path,
                          bool add_to_watch_list) OVERRIDE;
  virtual void AddServiceAtIndex(const std::string& service_path,
                                 size_t index,
                                 bool add_to_watch_list) OVERRIDE;
  virtual void RemoveService(const std::string& service_path) OVERRIDE;
  virtual void AddTechnology(const std::string& type, bool enabled) OVERRIDE;
  virtual void RemoveTechnology(const std::string& type) OVERRIDE;
  virtual void AddGeoNetwork(const std::string& technology,
                             const base::DictionaryValue& network) OVERRIDE;
  virtual void ClearProperties() OVERRIDE;

 private:
  void AddServiceToWatchList(const std::string& service_path);
  void SetDefaultProperties();
  void PassStubProperties(const DictionaryValueCallback& callback) const;
  void PassStubGeoNetworks(const DictionaryValueCallback& callback) const;
  void CallNotifyObserversPropertyChanged(const std::string& property,
                                          int delay_ms);
  void NotifyObserversPropertyChanged(const std::string& property);
  base::ListValue* GetListProperty(const std::string& property);
  bool TechnologyEnabled(const std::string& type) const;
  base::ListValue* GetEnabledServiceList(const std::string& property) const;

  // Dictionary of property name -> property value
  base::DictionaryValue stub_properties_;
  // Dictionary of technology -> list of property dictionaries
  base::DictionaryValue stub_geo_networks_;

  ObserverList<ShillPropertyChangedObserver> observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillManagerClientStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillManagerClientStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_MANAGER_CLIENT_STUB_H_

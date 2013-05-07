// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"

namespace chromeos {

// A fake implementation of ShillManagerClient. This class does nothing.
class FakeShillManagerClient : public ShillManagerClient {
 public:
  FakeShillManagerClient();
  virtual ~FakeShillManagerClient();

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
  virtual void EnableTechnology(const std::string& type,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback) OVERRIDE;
  virtual void DisableTechnology(const std::string& type,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConfigureService(const base::DictionaryValue& properties,
                                const ObjectPathCallback& callback,
                                const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConfigureServiceForProfile(
      const dbus::ObjectPath& profile_path,
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void GetService(const base::DictionaryValue& properties,
                          const ObjectPathCallback& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyDestination(const VerificationProperties& properties,
                                 const BooleanCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptCredentials(
      const VerificationProperties& properties,
      const std::string& service_path,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptData(
      const VerificationProperties& properties,
      const std::string& data,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectToBestServices(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual TestInterface* GetTestInterface() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeShillManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_

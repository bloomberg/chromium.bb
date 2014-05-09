// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_IPCONFIG_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_IPCONFIG_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_ipconfig_client.h"

namespace chromeos {

// A fake implementation of ShillIPConfigClient.
class CHROMEOS_EXPORT FakeShillIPConfigClient
    : public ShillIPConfigClient,
      public ShillIPConfigClient::TestInterface {
 public:
  FakeShillIPConfigClient();
  virtual ~FakeShillIPConfigClient();

  // ShillIPConfigClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void Refresh(const dbus::ObjectPath& ipconfig_path,
                       const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual ShillIPConfigClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillIPConfigClient::TestInterface overrides.
  virtual void AddIPConfig(const std::string& ip_config_path,
                           const base::DictionaryValue& properties) OVERRIDE;

 private:
  // Runs callback with |values|.
  void PassProperties(const base::DictionaryValue* values,
                      const DictionaryValueCallback& callback) const;

  // Dictionary of <ipconfig_path, property dictionaries>
  base::DictionaryValue ipconfigs_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillIPConfigClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillIPConfigClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_IPCONFIG_CLIENT_H_

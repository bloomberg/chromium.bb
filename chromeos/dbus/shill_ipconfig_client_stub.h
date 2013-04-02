// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_IPCONFIG_CLIENT_STUB_H_
#define CHROMEOS_DBUS_SHILL_IPCONFIG_CLIENT_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/dbus/shill_ipconfig_client.h"

namespace chromeos {

// A stub implementation of ShillIPConfigClient.
class ShillIPConfigClientStub : public ShillIPConfigClient {
 public:
  ShillIPConfigClientStub();
  virtual ~ShillIPConfigClientStub();

  // ShillIPConfigClient overrides:
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
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE;

 private:
  // Runs callback with |values|.
  void PassProperties(const base::DictionaryValue* values,
                      const DictionaryValueCallback& callback) const;

  // Dictionary of <ipconfig_path, property dictionaries>
  base::DictionaryValue ipconfigs_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillIPConfigClientStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillIPConfigClientStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_IPCONFIG_CLIENT_STUB_H_

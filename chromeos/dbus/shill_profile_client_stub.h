// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_PROFILE_CLIENT_STUB_H_
#define CHROMEOS_DBUS_SHILL_PROFILE_CLIENT_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/dbus/shill_profile_client.h"

namespace chromeos {

// A stub implementation of ShillProfileClient.
class ShillProfileClientStub : public ShillProfileClient {
 public:
  ShillProfileClientStub();
  virtual ~ShillProfileClientStub();

  // ShillProfileClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(
      const dbus::ObjectPath& profile_path,
      const DictionaryValueCallbackWithoutStatus& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void GetEntry(const dbus::ObjectPath& profile_path,
                        const std::string& entry_path,
                        const DictionaryValueCallbackWithoutStatus& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;

 private:
  void PassEmptyDictionaryValue(
      const DictionaryValueCallbackWithoutStatus& callback) const;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillProfileClientStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillProfileClientStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_PROFILE_CLIENT_STUB_H_

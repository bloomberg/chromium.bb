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
class ShillProfileClientStub : public ShillProfileClient,
                               public ShillProfileClient::TestInterface {
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
  virtual ShillProfileClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillProfileClient::TestInterface overrides.
  virtual void AddProfile(const std::string& profile_path) OVERRIDE;
  virtual void AddEntry(const std::string& profile_path,
                        const std::string& entry_path,
                        const base::DictionaryValue& properties) OVERRIDE;
  virtual bool AddService(const std::string& service_path) OVERRIDE;

 private:
  base::DictionaryValue* GetProfile(const dbus::ObjectPath& profile_path,
                                    const ErrorCallback& error_callback);

  // This maps profile path -> entry path -> Shill properties.
  base::DictionaryValue profile_entries_;

  DISALLOW_COPY_AND_ASSIGN(ShillProfileClientStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_PROFILE_CLIENT_STUB_H_

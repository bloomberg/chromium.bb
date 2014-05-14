// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_PROFILE_CLIENT_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"

namespace chromeos {

// A stub implementation of ShillProfileClient.
class CHROMEOS_EXPORT FakeShillProfileClient :
      public ShillProfileClient,
      public ShillProfileClient::TestInterface {
 public:
  FakeShillProfileClient();
  virtual ~FakeShillProfileClient();

  // ShillProfileClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
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
  virtual void AddProfile(const std::string& profile_path,
                          const std::string& userhash) OVERRIDE;
  virtual void AddEntry(const std::string& profile_path,
                        const std::string& entry_path,
                        const base::DictionaryValue& properties) OVERRIDE;
  virtual bool AddService(const std::string& profile_path,
                          const std::string& service_path) OVERRIDE;
  virtual bool UpdateService(const std::string& profile_path,
                             const std::string& service_path) OVERRIDE;
  virtual void GetProfilePaths(std::vector<std::string>* profiles) OVERRIDE;
  virtual bool GetService(const std::string& service_path,
                          std::string* profile_path,
                          base::DictionaryValue* properties) OVERRIDE;
  virtual void ClearProfiles() OVERRIDE;

 private:
  struct ProfileProperties;
  typedef std::map<std::string, ProfileProperties*> ProfileMap;

  bool AddOrUpdateServiceImpl(const std::string& profile_path,
                              const std::string& service_path,
                              ProfileProperties* profile);

  ProfileProperties* GetProfile(const dbus::ObjectPath& profile_path,
                                const ErrorCallback& error_callback);

  // The values are owned by this class and are explicitly destroyed where
  // necessary.
  ProfileMap profiles_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillProfileClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_PROFILE_CLIENT_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_profile_client_stub.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

struct ShillProfileClientStub::ProfileProperties {
  base::DictionaryValue entries;
  base::DictionaryValue properties;
};

namespace {

void PassDictionary(
    const ShillProfileClient::DictionaryValueCallbackWithoutStatus& callback,
    const base::DictionaryValue* dictionary) {
  callback.Run(*dictionary);
}

}  // namespace

// static
const char ShillProfileClientStub::kSharedProfilePath[] = "/profile/default";

ShillProfileClientStub::ShillProfileClientStub() {
  AddProfile(kSharedProfilePath, std::string());
}

ShillProfileClientStub::~ShillProfileClientStub() {
  STLDeleteValues(&profiles_);
}

void ShillProfileClientStub::AddPropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillProfileClientStub::RemovePropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillProfileClientStub::GetProperties(
    const dbus::ObjectPath& profile_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  scoped_ptr<base::DictionaryValue> properties(profile->properties.DeepCopy());
  base::ListValue* entry_paths = new base::ListValue;
  properties->SetWithoutPathExpansion(flimflam::kEntriesProperty, entry_paths);
  for (base::DictionaryValue::Iterator it(profile->entries); !it.IsAtEnd();
       it.Advance()) {
    entry_paths->AppendString(it.key());
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PassDictionary, callback, base::Owned(properties.release())));
}

void ShillProfileClientStub::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  base::DictionaryValue* entry = NULL;
  profile->entries.GetDictionaryWithoutPathExpansion(entry_path, &entry);
  if (!entry) {
    error_callback.Run("Error.InvalidProfileEntry", "Invalid profile entry");
    return;
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PassDictionary, callback, base::Owned(entry->DeepCopy())));
}

void ShillProfileClientStub::DeleteEntry(const dbus::ObjectPath& profile_path,
                                         const std::string& entry_path,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  if (!profile->entries.RemoveWithoutPathExpansion(entry_path, NULL)) {
    error_callback.Run("Error.InvalidProfileEntry", entry_path);
    return;
  }

  base::StringValue profile_path_value("");
  DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()->
      SetServiceProperty(entry_path,
                         flimflam::kProfileProperty,
                         profile_path_value);

  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

ShillProfileClient::TestInterface* ShillProfileClientStub::GetTestInterface() {
  return this;
}

void ShillProfileClientStub::AddProfile(const std::string& profile_path,
                                        const std::string& userhash) {
  if (GetProfile(dbus::ObjectPath(profile_path), ErrorCallback()))
    return;

  ProfileProperties* profile = new ProfileProperties;
  profile->properties.SetStringWithoutPathExpansion(shill::kUserHashProperty,
                                                    userhash);
  profiles_[profile_path] = profile;
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddProfile(profile_path);
}

void ShillProfileClientStub::AddEntry(const std::string& profile_path,
                                      const std::string& entry_path,
                                      const base::DictionaryValue& properties) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path),
                                          ErrorCallback());
  DCHECK(profile);
  profile->entries.SetWithoutPathExpansion(entry_path,
                                           properties.DeepCopy());
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddManagerService(entry_path, false /* visible */, false /* watch */);
}

bool ShillProfileClientStub::AddService(const std::string& profile_path,
                                        const std::string& service_path) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path),
                                          ErrorCallback());
  if (!profile) {
    LOG(ERROR) << "No matching profile: " << profile_path;
    return false;
  }

  ShillServiceClient::TestInterface* service_test =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  const base::DictionaryValue* service_properties =
      service_test->GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "No matching service: " << service_path;
    return false;
  }
  std::string service_profile_path;
  service_properties->GetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                                    &service_profile_path);
  if (!service_profile_path.empty() && service_profile_path != profile_path) {
    LOG(ERROR) << "Service has non matching profile path: "
               << service_profile_path;
    return false;
  }

  base::StringValue profile_path_value(profile_path);
  service_test->SetServiceProperty(service_path,
                                   flimflam::kProfileProperty,
                                   profile_path_value);
  profile->entries.SetWithoutPathExpansion(service_path,
                                           service_properties->DeepCopy());
  return true;
}

void ShillProfileClientStub::GetProfilePaths(
    std::vector<std::string>* profiles) {
  for (ProfileMap::iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    profiles->push_back(iter->first);
  }
}

ShillProfileClientStub::ProfileProperties* ShillProfileClientStub::GetProfile(
    const dbus::ObjectPath& profile_path,
    const ErrorCallback& error_callback) {
  ProfileMap::const_iterator found = profiles_.find(profile_path.value());
  if (found == profiles_.end()) {
    if (!error_callback.is_null())
      error_callback.Run("Error.InvalidProfile", "Invalid profile");
    return NULL;
  }

  return found->second;
}

}  // namespace chromeos

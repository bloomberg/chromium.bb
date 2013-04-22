// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_profile_client_stub.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kSharedProfilePath[] = "/profile/default";

void PassEmptyDictionary(
    const ShillProfileClient::DictionaryValueCallbackWithoutStatus& callback) {
  base::DictionaryValue dictionary;
  if (callback.is_null())
    return;
  callback.Run(dictionary);
}

void PassDictionary(
    const ShillProfileClient::DictionaryValueCallbackWithoutStatus& callback,
    const base::DictionaryValue* dictionary) {
  if (callback.is_null())
    return;
  callback.Run(*dictionary);
}

base::DictionaryValue* GetOrCreateDictionary(const std::string& key,
                                             base::DictionaryValue* dict) {
  base::DictionaryValue* nested_dict = NULL;
  dict->GetDictionaryWithoutPathExpansion(key, &nested_dict);
  if (!nested_dict) {
    nested_dict = new base::DictionaryValue;
    dict->SetWithoutPathExpansion(key, nested_dict);
  }
  return nested_dict;
}

}  // namespace

ShillProfileClientStub::ShillProfileClientStub() {
  AddProfile(kSharedProfilePath);
}

ShillProfileClientStub::~ShillProfileClientStub() {
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
  base::DictionaryValue* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  base::ListValue* entry_paths = new base::ListValue;
  properties->SetWithoutPathExpansion(flimflam::kEntriesProperty, entry_paths);
  for (base::DictionaryValue::Iterator it(*profile); !it.IsAtEnd();
       it.Advance()) {
    entry_paths->AppendString(it.key());
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PassDictionary, callback, base::Owned(properties.release())));
}

void ShillProfileClientStub::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  base::DictionaryValue* entry = NULL;
  profile->GetDictionaryWithoutPathExpansion(entry_path, &entry);
  if (!entry) {
    error_callback.Run("Error.InvalidProfileEntry", "Invalid profile entry");
    return;
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PassDictionary, callback, base::Owned(entry->DeepCopy())));
}

void ShillProfileClientStub::DeleteEntry(const dbus::ObjectPath& profile_path,
                                         const std::string& entry_path,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  base::DictionaryValue* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  if (!profile->RemoveWithoutPathExpansion(entry_path, NULL)) {
    error_callback.Run("Error.InvalidProfileEntry", "Invalid profile entry");
    return;
  }

  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

ShillProfileClient::TestInterface* ShillProfileClientStub::GetTestInterface() {
  return this;
}

void ShillProfileClientStub::AddProfile(const std::string& profile_path) {
  profile_entries_.SetWithoutPathExpansion(profile_path,
                                           new base::DictionaryValue);
}

void ShillProfileClientStub::AddEntry(const std::string& profile_path,
                                      const std::string& entry_path,
                                      const base::DictionaryValue& properties) {
  base::DictionaryValue* profile = GetOrCreateDictionary(profile_path,
                                                         &profile_entries_);
  profile->SetWithoutPathExpansion(entry_path,
                                   properties.DeepCopy());
}

bool ShillProfileClientStub::AddService(const std::string& service_path) {
  ShillServiceClient::TestInterface* service_test =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  const base::DictionaryValue* properties =
      service_test->GetServiceProperties(service_path);
  std::string profile_path;
  if (!properties)
    return false;

  properties->GetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                            &profile_path);
  if (profile_path.empty())
    return false;

  AddEntry(profile_path, service_path, *properties);
  return true;
}

base::DictionaryValue* ShillProfileClientStub::GetProfile(
    const dbus::ObjectPath& profile_path,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* profile = NULL;
  profile_entries_.GetDictionaryWithoutPathExpansion(profile_path.value(),
                                                     &profile);
  if (!profile)
    error_callback.Run("Error.InvalidProfile", "Invalid profile");
  return profile;
}

}  // namespace chromeos

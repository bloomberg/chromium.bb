// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_service_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void PassStubListValue(const ShillServiceClient::ListValueCallback& callback,
                       base::ListValue* value) {
  callback.Run(*value);
}

void PassStubServiceProperties(
    const ShillServiceClient::DictionaryValueCallback& callback,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue* properties) {
  callback.Run(call_status, *properties);
}

void CallSortManagerServices() {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      SortManagerServices(true);
}

int GetInteractiveDelay() {
  return DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      GetInteractiveDelay();
}

}  // namespace

FakeShillServiceClient::FakeShillServiceClient() : weak_ptr_factory_(this) {
}

FakeShillServiceClient::~FakeShillServiceClient() {
}


// ShillServiceClient overrides.

void FakeShillServiceClient::Init(dbus::Bus* bus) {
}

void FakeShillServiceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).AddObserver(observer);
}

void FakeShillServiceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).RemoveObserver(observer);
}

void FakeShillServiceClient::GetProperties(
    const dbus::ObjectPath& service_path,
    const DictionaryValueCallback& callback) {
  base::DictionaryValue* nested_dict = NULL;
  std::unique_ptr<base::DictionaryValue> result_properties;
  DBusMethodCallStatus call_status;
  stub_services_.GetDictionaryWithoutPathExpansion(service_path.value(),
                                                   &nested_dict);
  if (nested_dict) {
    result_properties.reset(nested_dict->DeepCopy());
    // Remove credentials that Shill wouldn't send.
    result_properties->RemoveWithoutPathExpansion(shill::kPassphraseProperty,
                                                  NULL);
    call_status = DBUS_METHOD_CALL_SUCCESS;
  } else {
    // This may happen if we remove services from the list.
    VLOG(2) << "Properties not found for: " << service_path.value();
    result_properties.reset(new base::DictionaryValue);
    call_status = DBUS_METHOD_CALL_FAILURE;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PassStubServiceProperties, callback, call_status,
                            base::Owned(result_properties.release())));
}

void FakeShillServiceClient::SetProperty(const dbus::ObjectPath& service_path,
                                         const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  if (!SetServiceProperty(service_path.value(), name, value)) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::SetProperties(
    const dbus::ObjectPath& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    if (!SetServiceProperty(service_path.value(), iter.key(), iter.value())) {
      LOG(ERROR) << "Service not found: " << service_path.value();
      error_callback.Run("Error.InvalidService", "Invalid Service");
      return;
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ClearProperty(
    const dbus::ObjectPath& service_path,
    const std::string& name,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* dict = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(
      service_path.value(), &dict)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  dict->RemoveWithoutPathExpansion(name, NULL);
  // Note: Shill does not send notifications when properties are cleared.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ClearProperties(
    const dbus::ObjectPath& service_path,
    const std::vector<std::string>& names,
    const ListValueCallback& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* dict = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(
      service_path.value(), &dict)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  std::unique_ptr<base::ListValue> results(new base::ListValue);
  for (std::vector<std::string>::const_iterator iter = names.begin();
      iter != names.end(); ++iter) {
    dict->RemoveWithoutPathExpansion(*iter, NULL);
    // Note: Shill does not send notifications when properties are cleared.
    results->AppendBoolean(true);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PassStubListValue, callback, base::Owned(results.release())));
}

void FakeShillServiceClient::Connect(const dbus::ObjectPath& service_path,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  VLOG(1) << "FakeShillServiceClient::Connect: " << service_path.value();
  base::DictionaryValue* service_properties = NULL;
  if (!stub_services_.GetDictionary(service_path.value(),
                                    &service_properties)) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }

  // Set any other services of the same Type to 'offline' first, before setting
  // State to Association which will trigger sorting Manager.Services and
  // sending an update.
  SetOtherServicesOffline(service_path.value());

  // Clear Error.
  service_properties->SetStringWithoutPathExpansion(shill::kErrorProperty, "");

  // Set Associating.
  base::StringValue associating_value(shill::kStateAssociation);
  SetServiceProperty(service_path.value(), shill::kStateProperty,
                     associating_value);

  // Stay Associating until the state is changed again after a delay.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillServiceClient::ContinueConnect,
                 weak_ptr_factory_.GetWeakPtr(), service_path.value()),
      base::TimeDelta::FromSeconds(GetInteractiveDelay()));

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::Disconnect(const dbus::ObjectPath& service_path,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  base::Value* service;
  if (!stub_services_.Get(service_path.value(), &service)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  // Set Idle after a delay
  base::StringValue idle_value(shill::kStateIdle);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&FakeShillServiceClient::SetProperty,
                            weak_ptr_factory_.GetWeakPtr(), service_path,
                            shill::kStateProperty, idle_value,
                            base::Bind(&base::DoNothing), error_callback),
      base::TimeDelta::FromSeconds(GetInteractiveDelay()));
  callback.Run();
}

void FakeShillServiceClient::Remove(const dbus::ObjectPath& service_path,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ActivateCellularModem(
    const dbus::ObjectPath& service_path,
    const std::string& carrier,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* service_properties =
      GetModifiableServiceProperties(service_path.value(), false);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
  }
  SetServiceProperty(service_path.value(),
                     shill::kActivationStateProperty,
                     base::StringValue(shill::kActivationStateActivating));
  // Set Activated after a delay
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillServiceClient::SetCellularActivated,
                 weak_ptr_factory_.GetWeakPtr(), service_path, error_callback),
      base::TimeDelta::FromSeconds(GetInteractiveDelay()));

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::CompleteCellularActivation(
    const dbus::ObjectPath& service_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::GetLoadableProfileEntries(
    const dbus::ObjectPath& service_path,
    const DictionaryValueCallback& callback) {
  // Provide a dictionary with a single { profile_path, service_path } entry
  // if the Profile property is set, or an empty dictionary.
  std::unique_ptr<base::DictionaryValue> result_properties(
      new base::DictionaryValue);
  base::DictionaryValue* service_properties =
      GetModifiableServiceProperties(service_path.value(), false);
  if (service_properties) {
    std::string profile_path;
    if (service_properties->GetStringWithoutPathExpansion(
            shill::kProfileProperty, &profile_path)) {
      result_properties->SetStringWithoutPathExpansion(
          profile_path, service_path.value());
    }
  } else {
    LOG(WARNING) << "Service not in profile: " << service_path.value();
  }

  DBusMethodCallStatus call_status = DBUS_METHOD_CALL_SUCCESS;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PassStubServiceProperties, callback, call_status,
                            base::Owned(result_properties.release())));
}

ShillServiceClient::TestInterface* FakeShillServiceClient::GetTestInterface() {
  return this;
}

// ShillServiceClient::TestInterface overrides.

void FakeShillServiceClient::AddService(const std::string& service_path,
                                        const std::string& guid,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::string& state,
                                        bool visible) {
  AddServiceWithIPConfig(service_path, guid, name,
                         type, state, "" /* ipconfig_path */,
                         visible);
}

void FakeShillServiceClient::AddServiceWithIPConfig(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    const std::string& ipconfig_path,
    bool visible) {
  base::DictionaryValue* properties = SetServiceProperties(
      service_path, guid, name, type, state, visible);

  std::string profile_path;
  if (properties->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                &profile_path) &&
      !profile_path.empty()) {
    DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface()->
        UpdateService(profile_path, service_path);
  }

  if (!ipconfig_path.empty()) {
    properties->SetWithoutPathExpansion(
        shill::kIPConfigProperty,
        new base::StringValue(ipconfig_path));
  }

  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddManagerService(service_path, true);
}

base::DictionaryValue* FakeShillServiceClient::SetServiceProperties(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    bool visible) {
  base::DictionaryValue* properties =
      GetModifiableServiceProperties(service_path, true);
  connect_behavior_.erase(service_path);

  std::string profile_path;
  base::DictionaryValue profile_properties;
  if (DBusThreadManager::Get()
          ->GetShillProfileClient()
          ->GetTestInterface()
          ->GetService(service_path, &profile_path, &profile_properties)) {
    properties->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                              profile_path);
  }

  // If |guid| is provided, set Service.GUID to that. Otherwise if a GUID is
  // stored in a profile entry, use that. Otherwise leave it blank. Shill does
  // not enforce a valid guid, we do that at the NetworkStateHandler layer.
  std::string guid_to_set = guid;
  if (guid_to_set.empty()) {
    profile_properties.GetStringWithoutPathExpansion(shill::kGuidProperty,
                                                     &guid_to_set);
  }
  if (!guid_to_set.empty()) {
    properties->SetStringWithoutPathExpansion(shill::kGuidProperty,
                                              guid_to_set);
  }
  properties->SetStringWithoutPathExpansion(shill::kSSIDProperty, name);
  shill_property_util::SetSSID(name, properties);  // Sets kWifiHexSsid
  properties->SetStringWithoutPathExpansion(shill::kNameProperty, name);
  std::string device_path = DBusThreadManager::Get()
                                ->GetShillDeviceClient()
                                ->GetTestInterface()
                                ->GetDevicePathForType(type);
  properties->SetStringWithoutPathExpansion(shill::kDeviceProperty,
                                            device_path);
  properties->SetStringWithoutPathExpansion(shill::kTypeProperty, type);
  properties->SetStringWithoutPathExpansion(shill::kStateProperty, state);
  properties->SetBooleanWithoutPathExpansion(shill::kVisibleProperty, visible);
  if (type == shill::kTypeWifi) {
    properties->SetStringWithoutPathExpansion(shill::kSecurityClassProperty,
                                              shill::kSecurityNone);
    properties->SetStringWithoutPathExpansion(shill::kModeProperty,
                                              shill::kModeManaged);
  }
  return properties;
}

void FakeShillServiceClient::RemoveService(const std::string& service_path) {
  stub_services_.RemoveWithoutPathExpansion(service_path, NULL);
  connect_behavior_.erase(service_path);
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      RemoveManagerService(service_path);
}

bool FakeShillServiceClient::SetServiceProperty(const std::string& service_path,
                                                const std::string& property,
                                                const base::Value& value) {
  base::DictionaryValue* dict = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(service_path, &dict))
    return false;

  VLOG(1) << "Service.SetProperty: " << property << " = " << value
          << " For: " << service_path;

  base::DictionaryValue new_properties;
  std::string changed_property;
  base::CompareCase case_sensitive = base::CompareCase::SENSITIVE;
  if (base::StartsWith(property, "Provider.", case_sensitive) ||
      base::StartsWith(property, "OpenVPN.", case_sensitive) ||
      base::StartsWith(property, "L2TPIPsec.", case_sensitive)) {
    // These properties are only nested within the Provider dictionary if read
    // from Shill. Properties that start with "Provider" need to have that
    // stripped off, other properties are nested in the "Provider" dictionary
    // as-is.
    std::string key = property;
    if (base::StartsWith(property, "Provider.", case_sensitive))
      key = property.substr(strlen("Provider."));
    base::DictionaryValue* provider = new base::DictionaryValue;
    provider->SetWithoutPathExpansion(key, value.DeepCopy());
    new_properties.SetWithoutPathExpansion(shill::kProviderProperty, provider);
    changed_property = shill::kProviderProperty;
  } else if (value.GetType() == base::Value::Type::DICTIONARY) {
    const base::DictionaryValue* new_dict = NULL;
    value.GetAsDictionary(&new_dict);
    CHECK(new_dict);
    std::unique_ptr<base::Value> cur_value;
    base::DictionaryValue* cur_dict;
    if (dict->RemoveWithoutPathExpansion(property, &cur_value) &&
        cur_value->GetAsDictionary(&cur_dict)) {
      cur_dict->Clear();
      cur_dict->MergeDictionary(new_dict);
      new_properties.SetWithoutPathExpansion(property, cur_value.release());
    } else {
      new_properties.SetWithoutPathExpansion(property, value.DeepCopy());
    }
    changed_property = property;
  } else {
    new_properties.SetWithoutPathExpansion(property, value.DeepCopy());
    changed_property = property;
  }

  dict->MergeDictionary(&new_properties);

  // Add or update the profile entry.
  ShillProfileClient::TestInterface* profile_test =
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
  if (property == shill::kProfileProperty) {
    std::string profile_path;
    if (value.GetAsString(&profile_path)) {
      if (!profile_path.empty())
        profile_test->AddService(profile_path, service_path);
    } else {
      LOG(ERROR) << "Profile value is not a String!";
    }
  } else {
    std::string profile_path;
    if (dict->GetStringWithoutPathExpansion(
            shill::kProfileProperty, &profile_path) && !profile_path.empty()) {
      profile_test->UpdateService(profile_path, service_path);
    }
  }

  // Notify the Manager if the state changed (affects DefaultService).
  if (property == shill::kStateProperty) {
    std::string state;
    value.GetAsString(&state);
    DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
        ServiceStateChanged(service_path, state);
  }

  // If the State or Visibility changes, the sort order of service lists may
  // change and the DefaultService property may change.
  if (property == shill::kStateProperty ||
      property == shill::kVisibleProperty) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CallSortManagerServices));
  }

  // Notifiy Chrome of the property change.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeShillServiceClient::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), dbus::ObjectPath(service_path),
                 changed_property));
  return true;
}

const base::DictionaryValue* FakeShillServiceClient::GetServiceProperties(
    const std::string& service_path) const {
  const base::DictionaryValue* properties = NULL;
  stub_services_.GetDictionaryWithoutPathExpansion(service_path, &properties);
  return properties;
}

void FakeShillServiceClient::ClearServices() {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      ClearManagerServices();

  stub_services_.Clear();
  connect_behavior_.clear();
}

void FakeShillServiceClient::SetConnectBehavior(const std::string& service_path,
                                const base::Closure& behavior) {
  connect_behavior_[service_path] = behavior;
}

void FakeShillServiceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& service_path,
    const std::string& property) {
  base::DictionaryValue* dict = NULL;
  std::string path = service_path.value();
  if (!stub_services_.GetDictionaryWithoutPathExpansion(path, &dict)) {
    LOG(ERROR) << "Notify for unknown service: " << path;
    return;
  }
  base::Value* value = NULL;
  if (!dict->GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: "
               << path << " : " << property;
    return;
  }
  for (auto& observer : GetObserverList(service_path))
    observer.OnPropertyChanged(property, *value);
}

base::DictionaryValue* FakeShillServiceClient::GetModifiableServiceProperties(
    const std::string& service_path, bool create_if_missing) {
  base::DictionaryValue* properties = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(service_path,
                                                        &properties) &&
      create_if_missing) {
    properties = new base::DictionaryValue;
    stub_services_.Set(service_path, properties);
  }
  return properties;
}

FakeShillServiceClient::PropertyObserverList&
FakeShillServiceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  auto iter = observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = base::WrapUnique(observer_list);
  return *observer_list;
}

void FakeShillServiceClient::SetOtherServicesOffline(
    const std::string& service_path) {
  const base::DictionaryValue* service_properties = GetServiceProperties(
      service_path);
  if (!service_properties) {
    LOG(ERROR) << "Missing service: " << service_path;
    return;
  }
  std::string service_type;
  service_properties->GetString(shill::kTypeProperty, &service_type);
  // Set all other services of the same type to offline (Idle).
  for (base::DictionaryValue::Iterator iter(stub_services_);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string path = iter.key();
    if (path == service_path)
      continue;
    base::DictionaryValue* properties;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(path, &properties))
      NOTREACHED();

    std::string type;
    properties->GetString(shill::kTypeProperty, &type);
    if (type != service_type)
      continue;
    properties->SetWithoutPathExpansion(
        shill::kStateProperty,
        new base::StringValue(shill::kStateIdle));
  }
}

void FakeShillServiceClient::SetCellularActivated(
    const dbus::ObjectPath& service_path,
    const ErrorCallback& error_callback) {
  SetProperty(service_path,
              shill::kActivationStateProperty,
              base::StringValue(shill::kActivationStateActivated),
              base::Bind(&base::DoNothing),
              error_callback);
  SetProperty(service_path, shill::kConnectableProperty, base::Value(true),
              base::Bind(&base::DoNothing), error_callback);
}

void FakeShillServiceClient::ContinueConnect(const std::string& service_path) {
  VLOG(1) << "FakeShillServiceClient::ContinueConnect: " << service_path;
  base::DictionaryValue* service_properties = NULL;
  if (!stub_services_.GetDictionary(service_path, &service_properties)) {
    LOG(ERROR) << "Service not found: " << service_path;
    return;
  }

  if (base::ContainsKey(connect_behavior_, service_path)) {
    const base::Closure& custom_connect_behavior =
        connect_behavior_[service_path];
    VLOG(1) << "Running custom connect behavior for " << service_path;
    custom_connect_behavior.Run();
    return;
  }

  // No custom connect behavior set, continue with the default connect behavior.
  std::string passphrase;
  service_properties->GetStringWithoutPathExpansion(shill::kPassphraseProperty,
                                                    &passphrase);
  if (passphrase == "failure") {
    // Simulate a password failure.
    SetServiceProperty(service_path, shill::kErrorProperty,
                       base::StringValue(shill::kErrorBadPassphrase));
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::StringValue(shill::kStateFailure));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(&FakeShillServiceClient::SetServiceProperty),
            weak_ptr_factory_.GetWeakPtr(), service_path, shill::kErrorProperty,
            base::StringValue(shill::kErrorBadPassphrase)));
  } else {
    // Set Online.
    VLOG(1) << "Setting state to Online " << service_path;
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::StringValue(shill::kStateOnline));
  }
}

}  // namespace chromeos

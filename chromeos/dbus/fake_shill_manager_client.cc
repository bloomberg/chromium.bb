// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_manager_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Used to compare values for finding entries to erase in a ListValue.
// (ListValue only implements a const_iterator version of Find).
struct ValueEquals {
  explicit ValueEquals(const base::Value* first) : first_(first) {}
  bool operator()(const base::Value* second) const {
    return first_->Equals(second);
  }
  const base::Value* first_;
};

// Appends string entries from |service_list_in| whose entries in ServiceClient
// have Type |match_type| to either an active list or an inactive list
// based on the entry's State.
void AppendServicesForType(
    const base::ListValue* service_list_in,
    const char* match_type,
    std::vector<std::string>* active_service_list_out,
    std::vector<std::string>* inactive_service_list_out) {
  ShillServiceClient::TestInterface* service_client =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  for (base::ListValue::const_iterator iter = service_list_in->begin();
       iter != service_list_in->end(); ++iter) {
    std::string service_path;
    if (!(*iter)->GetAsString(&service_path))
      continue;
    const base::DictionaryValue* properties =
        service_client->GetServiceProperties(service_path);
    if (!properties) {
      LOG(ERROR) << "Properties not found for service: " << service_path;
      continue;
    }
    std::string type;
    properties->GetString(shill::kTypeProperty, &type);
    if (type != match_type)
      continue;
    std::string state;
    properties->GetString(shill::kStateProperty, &state);
    if (state == shill::kStateOnline ||
        state == shill::kStateAssociation ||
        state == shill::kStateConfiguration ||
        state == shill::kStatePortal ||
        state == shill::kStateReady) {
      active_service_list_out->push_back(service_path);
    } else {
      inactive_service_list_out->push_back(service_path);
    }
  }
}

}  // namespace

FakeShillManagerClient::FakeShillManagerClient()
    : weak_ptr_factory_(this) {
}

FakeShillManagerClient::~FakeShillManagerClient() {}

// ShillManagerClient overrides.

void FakeShillManagerClient::Init(dbus::Bus* bus) {}

void FakeShillManagerClient::AddPropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeShillManagerClient::RemovePropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeShillManagerClient::GetProperties(
    const DictionaryValueCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(
          &FakeShillManagerClient::PassStubProperties,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

void FakeShillManagerClient::GetNetworksForGeolocation(
    const DictionaryValueCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(
          &FakeShillManagerClient::PassStubGeoNetworks,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

void FakeShillManagerClient::SetProperty(const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  stub_properties_.SetWithoutPathExpansion(name, value.DeepCopy());
  CallNotifyObserversPropertyChanged(name, 0);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillManagerClient::RequestScan(const std::string& type,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  // For Stub purposes, default to a Wifi scan.
  std::string device_type = shill::kTypeWifi;
  if (!type.empty())
    device_type = type;
  ShillDeviceClient::TestInterface* device_client =
      DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
  std::string device_path = device_client->GetDevicePathForType(device_type);
  if (!device_path.empty()) {
    device_client->SetDeviceProperty(device_path,
                                     shill::kScanningProperty,
                                     base::FundamentalValue(true));
  }
  const int kScanDurationSeconds = 3;
  int scan_duration_seconds = kScanDurationSeconds;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableStubInteractive)) {
    scan_duration_seconds = 0;
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::ScanCompleted,
                 weak_ptr_factory_.GetWeakPtr(), device_path, callback),
      base::TimeDelta::FromSeconds(scan_duration_seconds));
}

void FakeShillManagerClient::EnableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = NULL;
  if (!stub_properties_.GetListWithoutPathExpansion(
      shill::kEnabledTechnologiesProperty, &enabled_list)) {
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, "StubError", "Property not found"));
    return;
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableStubInteractive)) {
    const int kEnableTechnologyDelaySeconds = 3;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeShillManagerClient::SetTechnologyEnabled,
                   weak_ptr_factory_.GetWeakPtr(), type, callback, true),
        base::TimeDelta::FromSeconds(kEnableTechnologyDelaySeconds));
  } else {
    SetTechnologyEnabled(type, callback, true);
  }
}

void FakeShillManagerClient::DisableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = NULL;
  if (!stub_properties_.GetListWithoutPathExpansion(
      shill::kEnabledTechnologiesProperty, &enabled_list)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, "StubError", "Property not found"));
    return;
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableStubInteractive)) {
    const int kDisableTechnologyDelaySeconds = 3;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeShillManagerClient::SetTechnologyEnabled,
                   weak_ptr_factory_.GetWeakPtr(), type, callback, false),
        base::TimeDelta::FromSeconds(kDisableTechnologyDelaySeconds));
  } else {
    SetTechnologyEnabled(type, callback, false);
  }
}

void FakeShillManagerClient::ConfigureService(
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  ShillServiceClient::TestInterface* service_client =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

  std::string guid;
  std::string type;
  if (!properties.GetString(shill::kGuidProperty, &guid) ||
      !properties.GetString(shill::kTypeProperty, &type)) {
    LOG(ERROR) << "ConfigureService requies GUID and Type to be defined";
    // If the properties aren't filled out completely, then just return an empty
    // object path.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, dbus::ObjectPath()));
    return;
  }

  // For the purposes of this stub, we're going to assume that the GUID property
  // is set to the service path because we don't want to re-implement Shill's
  // property matching magic here.
  std::string service_path = guid;

  std::string ipconfig_path;
  properties.GetString(shill::kIPConfigProperty, &ipconfig_path);

  // Merge the new properties with existing properties, if any.
  const base::DictionaryValue* existing_properties =
      service_client->GetServiceProperties(service_path);
  if (!existing_properties) {
    // Add a new service to the service client stub because none exists, yet.
    // This calls AddManagerService.
    service_client->AddServiceWithIPConfig(service_path, guid, type,
                                           shill::kStateIdle, ipconfig_path,
                                           true /* visible */,
                                           true /* watch */);
    existing_properties = service_client->GetServiceProperties(service_path);
  }

  scoped_ptr<base::DictionaryValue> merged_properties(
      existing_properties->DeepCopy());
  merged_properties->MergeDictionary(&properties);

  // Now set all the properties.
  for (base::DictionaryValue::Iterator iter(*merged_properties);
       !iter.IsAtEnd(); iter.Advance()) {
    service_client->SetServiceProperty(service_path, iter.key(), iter.value());
  }

  // If the Profile property is set, add it to ProfileClient.
  std::string profile_path;
  merged_properties->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile_path);
  if (!profile_path.empty()) {
    DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface()->
        AddService(profile_path, service_path);
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, dbus::ObjectPath(service_path)));
}

void FakeShillManagerClient::ConfigureServiceForProfile(
    const dbus::ObjectPath& profile_path,
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  std::string profile_property;
  properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                           &profile_property);
  CHECK(profile_property == profile_path.value());
  ConfigureService(properties, callback, error_callback);
}


void FakeShillManagerClient::GetService(
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, dbus::ObjectPath()));
}

void FakeShillManagerClient::VerifyDestination(
    const VerificationProperties& properties,
    const BooleanCallback& callback,
    const ErrorCallback& error_callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
}

void FakeShillManagerClient::VerifyAndEncryptCredentials(
    const VerificationProperties& properties,
    const std::string& service_path,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, "encrypted_credentials"));
}

void FakeShillManagerClient::VerifyAndEncryptData(
    const VerificationProperties& properties,
    const std::string& data,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback, "encrypted_data"));
}

void FakeShillManagerClient::ConnectToBestServices(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

ShillManagerClient::TestInterface* FakeShillManagerClient::GetTestInterface() {
  return this;
}

// ShillManagerClient::TestInterface overrides.

void FakeShillManagerClient::AddDevice(const std::string& device_path) {
  if (GetListProperty(shill::kDevicesProperty)->AppendIfNotPresent(
      base::Value::CreateStringValue(device_path))) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty, 0);
  }
}

void FakeShillManagerClient::RemoveDevice(const std::string& device_path) {
  base::StringValue device_path_value(device_path);
  if (GetListProperty(shill::kDevicesProperty)->Remove(
      device_path_value, NULL)) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty, 0);
  }
}

void FakeShillManagerClient::ClearDevices() {
  GetListProperty(shill::kDevicesProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kDevicesProperty, 0);
}

void FakeShillManagerClient::AddTechnology(const std::string& type,
                                           bool enabled) {
  if (GetListProperty(shill::kAvailableTechnologiesProperty)->
      AppendIfNotPresent(base::Value::CreateStringValue(type))) {
    CallNotifyObserversPropertyChanged(
        shill::kAvailableTechnologiesProperty, 0);
  }
  if (enabled &&
      GetListProperty(shill::kEnabledTechnologiesProperty)->
      AppendIfNotPresent(base::Value::CreateStringValue(type))) {
    CallNotifyObserversPropertyChanged(
        shill::kEnabledTechnologiesProperty, 0);
  }
}

void FakeShillManagerClient::RemoveTechnology(const std::string& type) {
  base::StringValue type_value(type);
  if (GetListProperty(shill::kAvailableTechnologiesProperty)->Remove(
      type_value, NULL)) {
    CallNotifyObserversPropertyChanged(
        shill::kAvailableTechnologiesProperty, 0);
  }
  if (GetListProperty(shill::kEnabledTechnologiesProperty)->Remove(
      type_value, NULL)) {
    CallNotifyObserversPropertyChanged(
        shill::kEnabledTechnologiesProperty, 0);
  }
}

void FakeShillManagerClient::SetTechnologyInitializing(const std::string& type,
                                                       bool initializing) {
  if (initializing) {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)->
        AppendIfNotPresent(base::Value::CreateStringValue(type))) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty, 0);
    }
  } else {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)->Remove(
            base::StringValue(type), NULL)) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty, 0);
    }
  }
}

void FakeShillManagerClient::ClearProperties() {
  stub_properties_.Clear();
}

void FakeShillManagerClient::AddManagerService(const std::string& service_path,
                                               bool add_to_visible_list,
                                               bool add_to_watch_list) {
  // Always add to ServiceCompleteListProperty.
  GetListProperty(shill::kServiceCompleteListProperty)->AppendIfNotPresent(
      base::Value::CreateStringValue(service_path));
  // If visible, add to Services and notify if new.
  if (add_to_visible_list &&
      GetListProperty(shill::kServicesProperty)->AppendIfNotPresent(
      base::Value::CreateStringValue(service_path))) {
    CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
  }
  if (add_to_watch_list)
    AddServiceToWatchList(service_path);
}

void FakeShillManagerClient::RemoveManagerService(
    const std::string& service_path) {
  base::StringValue service_path_value(service_path);
  if (GetListProperty(shill::kServicesProperty)->Remove(
      service_path_value, NULL)) {
    CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
  }
  GetListProperty(shill::kServiceCompleteListProperty)->Remove(
      service_path_value, NULL);
  if (GetListProperty(shill::kServiceWatchListProperty)->Remove(
      service_path_value, NULL)) {
    CallNotifyObserversPropertyChanged(
        shill::kServiceWatchListProperty, 0);
  }
}

void FakeShillManagerClient::ClearManagerServices() {
  GetListProperty(shill::kServicesProperty)->Clear();
  GetListProperty(shill::kServiceCompleteListProperty)->Clear();
  GetListProperty(shill::kServiceWatchListProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
  CallNotifyObserversPropertyChanged(shill::kServiceWatchListProperty, 0);
}

void FakeShillManagerClient::SortManagerServices() {
  static const char* ordered_types[] = {
    shill::kTypeEthernet,
    shill::kTypeWifi,
    shill::kTypeCellular,
    shill::kTypeWimax,
    shill::kTypeVPN
  };
  base::ListValue* service_list = GetListProperty(shill::kServicesProperty);
  if (!service_list || service_list->empty())
    return;
  std::vector<std::string> active_services;
  std::vector<std::string> inactive_services;
  for (size_t i = 0; i < arraysize(ordered_types); ++i) {
    AppendServicesForType(service_list, ordered_types[i],
                          &active_services, &inactive_services);
  }
  service_list->Clear();
  for (size_t i = 0; i < active_services.size(); ++i)
    service_list->AppendString(active_services[i]);
  for (size_t i = 0; i < inactive_services.size(); ++i)
    service_list->AppendString(inactive_services[i]);

  CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
}

void FakeShillManagerClient::AddGeoNetwork(
    const std::string& technology,
    const base::DictionaryValue& network) {
  base::ListValue* list_value = NULL;
  if (!stub_geo_networks_.GetListWithoutPathExpansion(
      technology, &list_value)) {
    list_value = new base::ListValue;
    stub_geo_networks_.SetWithoutPathExpansion(technology, list_value);
  }
  list_value->Append(network.DeepCopy());
}

void FakeShillManagerClient::AddProfile(const std::string& profile_path) {
  const char* key = shill::kProfilesProperty;
  if (GetListProperty(key)->AppendIfNotPresent(
          new base::StringValue(profile_path))) {
    CallNotifyObserversPropertyChanged(key, 0);
  }
}

void FakeShillManagerClient::AddServiceToWatchList(
    const std::string& service_path) {
  // Remove and insert the service, moving it to the front of the watch list.
  GetListProperty(shill::kServiceWatchListProperty)->Remove(
      base::StringValue(service_path), NULL);
  GetListProperty(shill::kServiceWatchListProperty)->Insert(
      0, base::Value::CreateStringValue(service_path));
  CallNotifyObserversPropertyChanged(
      shill::kServiceWatchListProperty, 0);
}

void FakeShillManagerClient::PassStubProperties(
    const DictionaryValueCallback& callback) const {
  scoped_ptr<base::DictionaryValue> stub_properties(
      stub_properties_.DeepCopy());
  // Remove disabled services from the list.
  stub_properties->SetWithoutPathExpansion(
      shill::kServicesProperty,
      GetEnabledServiceList(shill::kServicesProperty));
  stub_properties->SetWithoutPathExpansion(
      shill::kServiceWatchListProperty,
      GetEnabledServiceList(shill::kServiceWatchListProperty));
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *stub_properties);
}

void FakeShillManagerClient::PassStubGeoNetworks(
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, stub_geo_networks_);
}

void FakeShillManagerClient::CallNotifyObserversPropertyChanged(
    const std::string& property,
    int delay_ms) {
  // Avoid unnecessary delayed task if we have no observers (e.g. during
  // initial setup).
  if (!observer_list_.might_have_observers())
    return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableStubInteractive)) {
    delay_ms = 0;
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 property),
                 base::TimeDelta::FromMilliseconds(delay_ms));
}

void FakeShillManagerClient::NotifyObserversPropertyChanged(
    const std::string& property) {
  if (property == shill::kServicesProperty ||
      property == shill::kServiceWatchListProperty) {
    scoped_ptr<base::ListValue> services(GetEnabledServiceList(property));
    FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                      observer_list_,
                      OnPropertyChanged(property, *(services.get())));
    return;
  }
  base::Value* value = NULL;
  if (!stub_properties_.GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: " << property;
    return;
  }
  FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                    observer_list_,
                    OnPropertyChanged(property, *value));
}

base::ListValue* FakeShillManagerClient::GetListProperty(
    const std::string& property) {
  base::ListValue* list_property = NULL;
  if (!stub_properties_.GetListWithoutPathExpansion(
      property, &list_property)) {
    list_property = new base::ListValue;
    stub_properties_.SetWithoutPathExpansion(property, list_property);
  }
  return list_property;
}

bool FakeShillManagerClient::TechnologyEnabled(const std::string& type) const {
  if (type == shill::kTypeVPN)
    return true;  // VPN is always "enabled" since there is no associated device
  bool enabled = false;
  const base::ListValue* technologies;
  if (stub_properties_.GetListWithoutPathExpansion(
          shill::kEnabledTechnologiesProperty, &technologies)) {
    base::StringValue type_value(type);
    if (technologies->Find(type_value) != technologies->end())
      enabled = true;
  }
  return enabled;
}

void FakeShillManagerClient::SetTechnologyEnabled(
    const std::string& type,
    const base::Closure& callback,
    bool enabled) {
  base::ListValue* enabled_list = NULL;
  stub_properties_.GetListWithoutPathExpansion(
      shill::kEnabledTechnologiesProperty, &enabled_list);
  DCHECK(enabled_list);
  if (enabled)
    enabled_list->AppendIfNotPresent(new base::StringValue(type));
  else
    enabled_list->Remove(base::StringValue(type), NULL);
  CallNotifyObserversPropertyChanged(
      shill::kEnabledTechnologiesProperty, 0 /* already delayed */);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
  // May affect available services
  CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
  CallNotifyObserversPropertyChanged(shill::kServiceWatchListProperty, 0);
}

base::ListValue* FakeShillManagerClient::GetEnabledServiceList(
    const std::string& property) const {
  base::ListValue* new_service_list = new base::ListValue;
  const base::ListValue* service_list;
  if (stub_properties_.GetListWithoutPathExpansion(property, &service_list)) {
    ShillServiceClient::TestInterface* service_client =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    for (base::ListValue::const_iterator iter = service_list->begin();
         iter != service_list->end(); ++iter) {
      std::string service_path;
      if (!(*iter)->GetAsString(&service_path))
        continue;
      const base::DictionaryValue* properties =
          service_client->GetServiceProperties(service_path);
      if (!properties) {
        LOG(ERROR) << "Properties not found for service: " << service_path;
        continue;
      }
      std::string name;
      properties->GetString(shill::kNameProperty, &name);
      std::string type;
      properties->GetString(shill::kTypeProperty, &type);
      if (TechnologyEnabled(type))
        new_service_list->Append((*iter)->DeepCopy());
    }
  }
  return new_service_list;
}

void FakeShillManagerClient::ScanCompleted(const std::string& device_path,
                                           const base::Closure& callback) {
  if (!device_path.empty()) {
    DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface()->
        SetDeviceProperty(device_path,
                          shill::kScanningProperty,
                          base::FundamentalValue(false));
  }
  CallNotifyObserversPropertyChanged(shill::kServicesProperty, 0);
  CallNotifyObserversPropertyChanged(shill::kServiceWatchListProperty, 0);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

}  // namespace chromeos

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_manager_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
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
// have Type |match_type| to one of the output lists based on the entry's State.
void AppendServicesForType(
    const base::ListValue* service_list_in,
    const char* match_type,
    bool technology_enabled,
    std::vector<std::string>* active_service_list_out,
    std::vector<std::string>* inactive_service_list_out,
    std::vector<std::string>* disabled_service_list_out) {
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
    bool visible = false;
    if (technology_enabled)
      properties->GetBoolean(shill::kVisibleProperty, &visible);
    if (!visible) {
      disabled_service_list_out->push_back(service_path);
      continue;
    }
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

void LogErrorCallback(const std::string& error_name,
                      const std::string& error_message) {
  LOG(ERROR) << error_name << ": " << error_message;
}

bool IsConnectedState(const std::string& state) {
  return state == shill::kStateOnline || state == shill::kStatePortal ||
         state == shill::kStateReady;
}

void UpdatePortaledWifiState(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()
      ->SetServiceProperty(service_path,
                           shill::kStateProperty,
                           base::StringValue(shill::kStatePortal));
}

const char* kTechnologyUnavailable = "unavailable";
const char* kNetworkActivated = "activated";
const char* kNetworkDisabled = "disabled";
const char* kCellularServicePath = "/service/cellular1";

}  // namespace

// static
const char FakeShillManagerClient::kFakeEthernetNetworkGuid[] = "eth1_guid";

FakeShillManagerClient::FakeShillManagerClient()
    : interactive_delay_(0),
      weak_ptr_factory_(this) {
  ParseCommandLineSwitch();
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
  DVLOG(1) << "Manager.GetProperties";
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
  DVLOG(2) << "SetProperty: " << name;
  stub_properties_.SetWithoutPathExpansion(name, value.DeepCopy());
  CallNotifyObserversPropertyChanged(name);
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
    device_client->SetDeviceProperty(
        device_path, shill::kScanningProperty, base::FundamentalValue(true));
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::ScanCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path,
                 callback),
      base::TimeDelta::FromSeconds(interactive_delay_));
}

void FakeShillManagerClient::EnableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = NULL;
  if (!stub_properties_.GetListWithoutPathExpansion(
          shill::kAvailableTechnologiesProperty, &enabled_list)) {
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, "StubError", "Property not found"));
    return;
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::SetTechnologyEnabled,
                 weak_ptr_factory_.GetWeakPtr(),
                 type,
                 callback,
                 true),
      base::TimeDelta::FromSeconds(interactive_delay_));
}

void FakeShillManagerClient::DisableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = NULL;
  if (!stub_properties_.GetListWithoutPathExpansion(
          shill::kAvailableTechnologiesProperty, &enabled_list)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, "StubError", "Property not found"));
    return;
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::SetTechnologyEnabled,
                 weak_ptr_factory_.GetWeakPtr(),
                 type,
                 callback,
                 false),
      base::TimeDelta::FromSeconds(interactive_delay_));
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
    LOG(ERROR) << "ConfigureService requires GUID and Type to be defined";
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
    service_client->AddServiceWithIPConfig(service_path,
                                           guid /* guid */,
                                           guid /* name */,
                                           type,
                                           shill::kStateIdle,
                                           ipconfig_path,
                                           true /* visible */);
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
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, "encrypted_data"));
}

void FakeShillManagerClient::ConnectToBestServices(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (best_service_.empty()) {
    VLOG(1) << "No 'best' service set.";
    return;
  }

  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(best_service_), callback, error_callback);
}

ShillManagerClient::TestInterface* FakeShillManagerClient::GetTestInterface() {
  return this;
}

// ShillManagerClient::TestInterface overrides.

void FakeShillManagerClient::AddDevice(const std::string& device_path) {
  if (GetListProperty(shill::kDevicesProperty)
          ->AppendIfNotPresent(new base::StringValue(device_path))) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::RemoveDevice(const std::string& device_path) {
  base::StringValue device_path_value(device_path);
  if (GetListProperty(shill::kDevicesProperty)->Remove(
      device_path_value, NULL)) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::ClearDevices() {
  GetListProperty(shill::kDevicesProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
}

void FakeShillManagerClient::AddTechnology(const std::string& type,
                                           bool enabled) {
  if (GetListProperty(shill::kAvailableTechnologiesProperty)
          ->AppendIfNotPresent(new base::StringValue(type))) {
    CallNotifyObserversPropertyChanged(
        shill::kAvailableTechnologiesProperty);
  }
  if (enabled &&
      GetListProperty(shill::kEnabledTechnologiesProperty)
          ->AppendIfNotPresent(new base::StringValue(type))) {
    CallNotifyObserversPropertyChanged(
        shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::RemoveTechnology(const std::string& type) {
  base::StringValue type_value(type);
  if (GetListProperty(shill::kAvailableTechnologiesProperty)->Remove(
      type_value, NULL)) {
    CallNotifyObserversPropertyChanged(
        shill::kAvailableTechnologiesProperty);
  }
  if (GetListProperty(shill::kEnabledTechnologiesProperty)->Remove(
      type_value, NULL)) {
    CallNotifyObserversPropertyChanged(
        shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::SetTechnologyInitializing(const std::string& type,
                                                       bool initializing) {
  if (initializing) {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)
            ->AppendIfNotPresent(new base::StringValue(type))) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  } else {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)->Remove(
            base::StringValue(type), NULL)) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  }
}

void FakeShillManagerClient::AddGeoNetwork(
    const std::string& technology,
    const base::DictionaryValue& network) {
  base::ListValue* list_value = NULL;
  if (!stub_geo_networks_.GetListWithoutPathExpansion(technology,
                                                      &list_value)) {
    list_value = new base::ListValue;
    stub_geo_networks_.SetWithoutPathExpansion(technology, list_value);
  }
  list_value->Append(network.DeepCopy());
}

void FakeShillManagerClient::AddProfile(const std::string& profile_path) {
  const char* key = shill::kProfilesProperty;
  if (GetListProperty(key)
          ->AppendIfNotPresent(new base::StringValue(profile_path))) {
    CallNotifyObserversPropertyChanged(key);
  }
}

void FakeShillManagerClient::ClearProperties() {
  stub_properties_.Clear();
}

void FakeShillManagerClient::SetManagerProperty(const std::string& key,
                                                const base::Value& value) {
  SetProperty(key, value,
              base::Bind(&base::DoNothing), base::Bind(&LogErrorCallback));
}

void FakeShillManagerClient::AddManagerService(
    const std::string& service_path,
    bool notify_observers) {
  DVLOG(2) << "AddManagerService: " << service_path;
  GetListProperty(shill::kServiceCompleteListProperty)
      ->AppendIfNotPresent(new base::StringValue(service_path));
  SortManagerServices(false);
  if (notify_observers)
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::RemoveManagerService(
    const std::string& service_path) {
  DVLOG(2) << "RemoveManagerService: " << service_path;
  base::StringValue service_path_value(service_path);
  GetListProperty(shill::kServiceCompleteListProperty)->Remove(
      service_path_value, NULL);
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::ClearManagerServices() {
  DVLOG(1) << "ClearManagerServices";
  GetListProperty(shill::kServiceCompleteListProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::ServiceStateChanged(
    const std::string& service_path,
    const std::string& state) {
  if (service_path == default_service_ && !IsConnectedState(state)) {
    // Default service is no longer connected; clear.
    default_service_.clear();
    base::StringValue default_service_value(default_service_);
    SetManagerProperty(shill::kDefaultServiceProperty, default_service_value);
  }
}

void FakeShillManagerClient::SortManagerServices(bool notify) {
  DVLOG(1) << "SortManagerServices";
  static const char* ordered_types[] = {shill::kTypeEthernet,
                                        shill::kTypeWifi,
                                        shill::kTypeCellular,
                                        shill::kTypeWimax,
                                        shill::kTypeVPN};

  base::ListValue* complete_list =
      GetListProperty(shill::kServiceCompleteListProperty);
  if (complete_list->empty())
    return;
  scoped_ptr<base::ListValue> prev_complete_list(complete_list->DeepCopy());

  std::vector<std::string> active_services;
  std::vector<std::string> inactive_services;
  std::vector<std::string> disabled_services;
  for (size_t i = 0; i < arraysize(ordered_types); ++i) {
    AppendServicesForType(complete_list,
                          ordered_types[i],
                          TechnologyEnabled(ordered_types[i]),
                          &active_services,
                          &inactive_services,
                          &disabled_services);
  }
  complete_list->Clear();
  for (size_t i = 0; i < active_services.size(); ++i)
    complete_list->AppendString(active_services[i]);
  for (size_t i = 0; i < inactive_services.size(); ++i)
    complete_list->AppendString(inactive_services[i]);
  for (size_t i = 0; i < disabled_services.size(); ++i)
    complete_list->AppendString(disabled_services[i]);

  if (notify && !complete_list->Equals(prev_complete_list.get()))
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);

  // Set the first active service as the Default service.
  std::string new_default_service;
  if (!active_services.empty()) {
    ShillServiceClient::TestInterface* service_client =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    std::string service_path = active_services[0];
    const base::DictionaryValue* properties =
        service_client->GetServiceProperties(service_path);
    if (!properties) {
      LOG(ERROR) << "Properties not found for service: " << service_path;
    } else {
      std::string state;
      properties->GetString(shill::kStateProperty, &state);
      if (IsConnectedState(state))
        new_default_service = service_path;
    }
  }
  if (default_service_ != new_default_service) {
    default_service_ = new_default_service;
    base::StringValue default_service_value(default_service_);
    SetManagerProperty(shill::kDefaultServiceProperty, default_service_value);
  }
}

int FakeShillManagerClient::GetInteractiveDelay() const {
  return interactive_delay_;
}

void FakeShillManagerClient::SetBestServiceToConnect(
    const std::string& service_path) {
  best_service_ = service_path;
}

void FakeShillManagerClient::SetupDefaultEnvironment() {
  DBusThreadManager* dbus_manager = DBusThreadManager::Get();
  ShillServiceClient::TestInterface* services =
      dbus_manager->GetShillServiceClient()->GetTestInterface();
  DCHECK(services);
  ShillProfileClient::TestInterface* profiles =
      dbus_manager->GetShillProfileClient()->GetTestInterface();
  DCHECK(profiles);
  ShillDeviceClient::TestInterface* devices =
      dbus_manager->GetShillDeviceClient()->GetTestInterface();
  DCHECK(devices);
  ShillIPConfigClient::TestInterface* ip_configs =
      dbus_manager->GetShillIPConfigClient()->GetTestInterface();
  DCHECK(ip_configs);

  const std::string shared_profile = ShillProfileClient::GetSharedProfilePath();
  profiles->AddProfile(shared_profile, std::string());

  const bool add_to_visible = true;

  // IPConfigs
  base::DictionaryValue ipconfig_v4_dictionary;
  ipconfig_v4_dictionary.SetStringWithoutPathExpansion(
      shill::kAddressProperty, "0.0.0.0");
  ipconfig_v4_dictionary.SetStringWithoutPathExpansion(
      shill::kGatewayProperty, "0.0.0.1");
  ipconfig_v4_dictionary.SetIntegerWithoutPathExpansion(
      shill::kPrefixlenProperty, 0);
  ipconfig_v4_dictionary.SetStringWithoutPathExpansion(
      shill::kMethodProperty, shill::kTypeIPv4);
  ip_configs->AddIPConfig("ipconfig_v4_path", ipconfig_v4_dictionary);
  base::DictionaryValue ipconfig_v6_dictionary;
  ipconfig_v6_dictionary.SetStringWithoutPathExpansion(
      shill::kAddressProperty, "0:0:0:0:0:0:0:0");
  ipconfig_v6_dictionary.SetStringWithoutPathExpansion(
      shill::kMethodProperty, shill::kTypeIPv6);
  ip_configs->AddIPConfig("ipconfig_v6_path", ipconfig_v6_dictionary);

  bool enabled;
  std::string state;

  // Ethernet
  state = GetInitialStateForType(shill::kTypeEthernet, &enabled);
  if (state == shill::kStateOnline) {
    AddTechnology(shill::kTypeEthernet, enabled);
    devices->AddDevice(
        "/device/eth1", shill::kTypeEthernet, "stub_eth_device1");
    devices->SetDeviceProperty("/device/eth1",
                               shill::kAddressProperty,
                               base::StringValue("0123456789ab"));
    base::ListValue eth_ip_configs;
    eth_ip_configs.AppendString("ipconfig_v4_path");
    eth_ip_configs.AppendString("ipconfig_v6_path");
    devices->SetDeviceProperty("/device/eth1",
                               shill::kIPConfigsProperty,
                               eth_ip_configs);
    const std::string kFakeEthernetNetworkPath = "/service/eth1";
    services->AddService(kFakeEthernetNetworkPath,
                         kFakeEthernetNetworkGuid,
                         "eth1" /* name */,
                         shill::kTypeEthernet,
                         state,
                         add_to_visible);
    profiles->AddService(shared_profile, kFakeEthernetNetworkPath);
  }

  // Wifi
  state = GetInitialStateForType(shill::kTypeWifi, &enabled);
  if (state != kTechnologyUnavailable) {
    bool portaled = false;
    if (state == shill::kStatePortal) {
      portaled = true;
      state = shill::kStateIdle;
    }
    AddTechnology(shill::kTypeWifi, enabled);
    devices->AddDevice("/device/wifi1", shill::kTypeWifi, "stub_wifi_device1");
    devices->SetDeviceProperty("/device/wifi1",
                               shill::kAddressProperty,
                               base::StringValue("23456789abc"));
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.AppendString("ipconfig_v4_path");
    wifi_ip_configs.AppendString("ipconfig_v6_path");
    devices->SetDeviceProperty("/device/wifi1",
                               shill::kIPConfigsProperty,
                               wifi_ip_configs);

    const std::string kWifi1Path = "/service/wifi1";
    services->AddService(kWifi1Path,
                         "wifi1_guid",
                         "wifi1" /* name */,
                         shill::kTypeWifi,
                         state,
                         add_to_visible);
    services->SetServiceProperty(kWifi1Path,
                                 shill::kSecurityProperty,
                                 base::StringValue(shill::kSecurityWep));
    services->SetServiceProperty(kWifi1Path,
                                 shill::kConnectableProperty,
                                 base::FundamentalValue(true));
    profiles->AddService(shared_profile, kWifi1Path);

    const std::string kWifi2Path = "/service/wifi2";
    services->AddService(kWifi2Path,
                         "wifi2_PSK_guid",
                         "wifi2_PSK" /* name */,
                         shill::kTypeWifi,
                         shill::kStateIdle,
                         add_to_visible);
    services->SetServiceProperty(kWifi2Path,
                                 shill::kSecurityProperty,
                                 base::StringValue(shill::kSecurityPsk));

    base::FundamentalValue strength_value(80);
    services->SetServiceProperty(
        kWifi2Path, shill::kSignalStrengthProperty, strength_value);
    profiles->AddService(shared_profile, kWifi2Path);

    if (portaled) {
      const std::string kPortaledWifiPath = "/service/portaled_wifi";
      services->AddService(kPortaledWifiPath,
                           "portaled_wifi_guid",
                           "Portaled Wifi" /* name */,
                           shill::kTypeWifi,
                           shill::kStatePortal,
                           add_to_visible);
      services->SetServiceProperty(kPortaledWifiPath,
                                   shill::kSecurityProperty,
                                   base::StringValue(shill::kSecurityNone));
      services->SetConnectBehavior(kPortaledWifiPath,
                                   base::Bind(&UpdatePortaledWifiState,
                                              "portaled_wifi"));
      services->SetServiceProperty(kPortaledWifiPath,
                                   shill::kConnectableProperty,
                                   base::FundamentalValue(true));
      profiles->AddService(shared_profile, kPortaledWifiPath);
    }
  }

  // Wimax
  state = GetInitialStateForType(shill::kTypeWimax, &enabled);
  if (state != kTechnologyUnavailable) {
    AddTechnology(shill::kTypeWimax, enabled);
    devices->AddDevice(
        "/device/wimax1", shill::kTypeWimax, "stub_wimax_device1");

    services->AddService("/service/wimax1",
                         "wimax1_guid",
                         "wimax1" /* name */,
                         shill::kTypeWimax,
                         state,
                         add_to_visible);
    services->SetServiceProperty("/service/wimax1",
                                 shill::kConnectableProperty,
                                 base::FundamentalValue(true));
  }

  // Cellular
  state = GetInitialStateForType(shill::kTypeCellular, &enabled);
  if (state != kTechnologyUnavailable) {
    bool activated = false;
    if (state == kNetworkActivated) {
      activated = true;
      state = shill::kStateIdle;
    }
    AddTechnology(shill::kTypeCellular, enabled);
    devices->AddDevice(
        "/device/cellular1", shill::kTypeCellular, "stub_cellular_device1");
    devices->SetDeviceProperty("/device/cellular1",
                               shill::kCarrierProperty,
                               base::StringValue(shill::kCarrierSprint));

    services->AddService(kCellularServicePath,
                         "cellular1_guid",
                         "cellular1" /* name */,
                         shill::kTypeCellular,
                         state,
                         add_to_visible);
    base::StringValue technology_value(shill::kNetworkTechnologyGsm);
    services->SetServiceProperty(kCellularServicePath,
                                 shill::kNetworkTechnologyProperty,
                                 technology_value);

    if (activated) {
      services->SetServiceProperty(
          kCellularServicePath,
          shill::kActivationStateProperty,
          base::StringValue(shill::kActivationStateActivated));
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kConnectableProperty,
                                   base::FundamentalValue(true));
    } else {
      services->SetServiceProperty(
          kCellularServicePath,
          shill::kActivationStateProperty,
          base::StringValue(shill::kActivationStateNotActivated));
    }

    services->SetServiceProperty(kCellularServicePath,
                                 shill::kRoamingStateProperty,
                                 base::StringValue(shill::kRoamingStateHome));
  }

  // VPN
  state = GetInitialStateForType(shill::kTypeVPN, &enabled);
  if (state != kTechnologyUnavailable) {
    // Set the "Provider" dictionary properties. Note: when setting these in
    // Shill, "Provider.Type", etc keys are used, but when reading the values
    // "Provider" . "Type", etc keys are used. Here we are setting the values
    // that will be read (by the UI, tests, etc).
    base::DictionaryValue provider_properties;
    provider_properties.SetString(shill::kTypeProperty,
                                  shill::kProviderOpenVpn);
    provider_properties.SetString(shill::kHostProperty, "vpn_host");

    services->AddService("/service/vpn1",
                         "vpn1_guid",
                         "vpn1" /* name */,
                         shill::kTypeVPN,
                         state,
                         add_to_visible);
    services->SetServiceProperty(
        "/service/vpn1", shill::kProviderProperty, provider_properties);
    profiles->AddService(shared_profile, "/service/vpn1");

    services->AddService("/service/vpn2",
                         "vpn2_guid",
                         "vpn2" /* name */,
                         shill::kTypeVPN,
                         shill::kStateIdle,
                         add_to_visible);
    services->SetServiceProperty(
        "/service/vpn2", shill::kProviderProperty, provider_properties);
  }

  // Additional device states
  for (DevicePropertyMap::iterator iter1 = shill_device_property_map_.begin();
       iter1 != shill_device_property_map_.end(); ++iter1) {
    std::string device_type = iter1->first;
    std::string device_path = devices->GetDevicePathForType(device_type);
    for (ShillPropertyMap::iterator iter2 = iter1->second.begin();
         iter2 != iter1->second.end(); ++iter2) {
      devices->SetDeviceProperty(device_path, iter2->first, *(iter2->second));
      delete iter2->second;
    }
  }

  SortManagerServices(true);
}

// Private methods

void FakeShillManagerClient::PassStubProperties(
    const DictionaryValueCallback& callback) const {
  scoped_ptr<base::DictionaryValue> stub_properties(
      stub_properties_.DeepCopy());
  stub_properties->SetWithoutPathExpansion(
      shill::kServiceCompleteListProperty,
      GetEnabledServiceList(shill::kServiceCompleteListProperty));
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *stub_properties);
}

void FakeShillManagerClient::PassStubGeoNetworks(
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, stub_geo_networks_);
}

void FakeShillManagerClient::CallNotifyObserversPropertyChanged(
    const std::string& property) {
  // Avoid unnecessary delayed task if we have no observers (e.g. during
  // initial setup).
  if (!observer_list_.might_have_observers())
    return;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeShillManagerClient::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 property));
}

void FakeShillManagerClient::NotifyObserversPropertyChanged(
    const std::string& property) {
  DVLOG(1) << "NotifyObserversPropertyChanged: " << property;
  base::Value* value = NULL;
  if (!stub_properties_.GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: " << property;
    return;
  }
  if (property == shill::kServiceCompleteListProperty) {
    scoped_ptr<base::ListValue> services(GetEnabledServiceList(property));
    FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                      observer_list_,
                      OnPropertyChanged(property, *(services.get())));
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
  base::ListValue* enabled_list =
      GetListProperty(shill::kEnabledTechnologiesProperty);
  if (enabled)
    enabled_list->AppendIfNotPresent(new base::StringValue(type));
  else
    enabled_list->Remove(base::StringValue(type), NULL);
  CallNotifyObserversPropertyChanged(
      shill::kEnabledTechnologiesProperty);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
  // May affect available services.
  SortManagerServices(true);
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
  DVLOG(2) << "ScanCompleted";
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillManagerClient::ParseCommandLineSwitch() {
  // Default setup
  SetInitialNetworkState(shill::kTypeEthernet, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeWifi, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeCellular, shill::kStateIdle);
  SetInitialNetworkState(shill::kTypeVPN, shill::kStateIdle);

  // Parse additional options
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kShillStub))
    return;

  std::string option_str =
      command_line->GetSwitchValueASCII(switches::kShillStub);
  VLOG(1) << "Parsing command line:" << option_str;
  base::StringPairs string_pairs;
  base::SplitStringIntoKeyValuePairs(option_str, '=', ',', &string_pairs);
  for (base::StringPairs::iterator iter = string_pairs.begin();
       iter != string_pairs.end(); ++iter) {
    ParseOption((*iter).first, (*iter).second);
  }
}

bool FakeShillManagerClient::ParseOption(const std::string& arg0,
                                         const std::string& arg1) {
  VLOG(1) << "Parsing command line option: '" << arg0 << "=" << arg1 << "'";
  if ((arg0 == "clear" || arg0 == "reset") && arg1 == "1") {
    shill_initial_state_map_.clear();
    return true;
  } else if (arg0 == "interactive") {
    int seconds = 3;
    if (!arg1.empty())
      base::StringToInt(arg1, &seconds);
    interactive_delay_ = seconds;
    return true;
  } else if (arg0 == "sim_lock") {
    bool locked = (arg1 == "1") ? true : false;
    base::DictionaryValue* simlock_dict = new base::DictionaryValue;
    simlock_dict->Set(shill::kSIMLockEnabledProperty,
                      new base::FundamentalValue(locked));
  // TODO(stevenjb): Investigate why non-empty value breaks UI.
  std::string lock_type = "";  // shill::kSIMLockPin
    simlock_dict->SetString(shill::kSIMLockTypeProperty, lock_type);
    simlock_dict->SetInteger(shill::kSIMLockRetriesLeftProperty, 5);

    shill_device_property_map_
        [shill::kTypeCellular][shill::kSIMLockStatusProperty] = simlock_dict;
    shill_device_property_map_
        [shill::kTypeCellular][shill::kTechnologyFamilyProperty] =
            new base::StringValue(shill::kNetworkTechnologyGsm);
    return true;
  }
  return SetInitialNetworkState(arg0, arg1);
}

bool FakeShillManagerClient::SetInitialNetworkState(std::string type_arg,
                                                    std::string state_arg) {
  std::string state;
  state_arg = StringToLowerASCII(state_arg);
  if (state_arg.empty() || state_arg == "1" || state_arg == "on" ||
      state_arg == "enabled" || state_arg == "connected" ||
      state_arg == "online") {
    // Enabled and connected (default value)
    state = shill::kStateOnline;
  } else if (state_arg == "0" || state_arg == "off" ||
             state_arg == "inactive" || state_arg == shill::kStateIdle) {
    // Technology enabled, services are created but are not connected.
    state = shill::kStateIdle;
  } else if (state_arg == "disabled" || state_arg == "disconnect") {
    // Technology disabled but available, services created but not connected.
    state = kNetworkDisabled;
  } else if (state_arg == "none" || state_arg == "offline") {
    // Technology not available, do not create services.
    state = kTechnologyUnavailable;
  } else if (state_arg == "portal") {
    // Technology is enabled, a service is connected and in Portal state.
    state = shill::kStatePortal;
  } else if (state_arg == "active" || state_arg == "activated") {
    // Technology is enabled, a service is connected and Activated.
    state = kNetworkActivated;
  } else {
    LOG(ERROR) << "Unrecognized initial state: " << state_arg;
    return false;
  }

  type_arg = StringToLowerASCII(type_arg);
  // Special cases
  if (type_arg == "wireless") {
    shill_initial_state_map_[shill::kTypeWifi] = state;
    shill_initial_state_map_[shill::kTypeCellular] = state;
    return true;
  }
  // Convenience synonyms.
  if (type_arg == "eth")
    type_arg = shill::kTypeEthernet;

  if (type_arg != shill::kTypeEthernet &&
      type_arg != shill::kTypeWifi &&
      type_arg != shill::kTypeCellular &&
      type_arg != shill::kTypeWimax &&
      type_arg != shill::kTypeVPN) {
    LOG(WARNING) << "Unrecognized Shill network type: " << type_arg;
    return false;
  }

  // Unconnected or disabled ethernet is the same as unavailable.
  if (type_arg == shill::kTypeEthernet &&
      (state == shill::kStateIdle || state == kNetworkDisabled)) {
    state = kTechnologyUnavailable;
  }

  shill_initial_state_map_[type_arg] = state;
  return true;
}

std::string FakeShillManagerClient::GetInitialStateForType(
    const std::string& type,
    bool* enabled) {
  std::map<std::string, std::string>::const_iterator iter =
      shill_initial_state_map_.find(type);
  if (iter == shill_initial_state_map_.end()) {
    *enabled = false;
    return kTechnologyUnavailable;
  }
  std::string state = iter->second;
  if (state == kNetworkDisabled) {
    *enabled = false;
    return shill::kStateIdle;
  }
  *enabled = true;
  if ((state == shill::kStatePortal && type != shill::kTypeWifi) ||
      (state == kNetworkActivated && type != shill::kTypeCellular)) {
    LOG(WARNING) << "Invalid state: " << state << " for " << type;
    return shill::kStateIdle;
  }
  return state;
}

}  // namespace chromeos

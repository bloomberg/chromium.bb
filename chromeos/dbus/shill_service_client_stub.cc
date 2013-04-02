// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_service_client_stub.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ErrorFunction(const std::string& error_name,
                   const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

void PassStubListValue(const ShillServiceClient::ListValueCallback& callback,
                       base::ListValue* value) {
  callback.Run(*value);
}

}  // namespace

ShillServiceClientStub::ShillServiceClientStub() : weak_ptr_factory_(this) {
  SetDefaultProperties();
}

ShillServiceClientStub::~ShillServiceClientStub() {
  STLDeleteContainerPairSecondPointers(
      observer_list_.begin(), observer_list_.end());
}

// ShillServiceClient overrides.

void ShillServiceClientStub::AddPropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).AddObserver(observer);
}

void ShillServiceClientStub::RemovePropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).RemoveObserver(observer);
}

void ShillServiceClientStub::GetProperties(
    const dbus::ObjectPath& service_path,
    const DictionaryValueCallback& callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillServiceClientStub::PassStubServiceProperties,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_path,
                 callback));
}

void ShillServiceClientStub::SetProperty(const dbus::ObjectPath& service_path,
                                         const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  base::DictionaryValue* dict = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(
      service_path.value(), &dict)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  if (name == flimflam::kStateProperty) {
    // If we connect to a service, then we move it to the top of the list in
    // the manager client.
    std::string state;
    if (value.GetAsString(&state) && state == flimflam::kStateOnline) {
      ShillManagerClient* manager_client =
          DBusThreadManager::Get()->GetShillManagerClient();
      manager_client->GetTestInterface()->RemoveService(service_path.value());
      manager_client->GetTestInterface()->AddServiceAtIndex(
          service_path.value(), 0, true);
    }
  }
  dict->SetWithoutPathExpansion(name, value.DeepCopy());
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillServiceClientStub::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), service_path, name));
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillServiceClientStub::ClearProperty(
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
  dict->Remove(name, NULL);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillServiceClientStub::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), service_path, name));
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillServiceClientStub::ClearProperties(
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
  scoped_ptr<base::ListValue> results(new base::ListValue);
  for (std::vector<std::string>::const_iterator iter = names.begin();
      iter != names.end(); ++iter) {
    dict->Remove(*iter, NULL);
    results->AppendBoolean(true);
  }
  for (std::vector<std::string>::const_iterator iter = names.begin();
      iter != names.end(); ++iter) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &ShillServiceClientStub::NotifyObserversPropertyChanged,
            weak_ptr_factory_.GetWeakPtr(), service_path, *iter));
  }
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PassStubListValue,
                 callback, base::Owned(results.release())));
}

void ShillServiceClientStub::Connect(const dbus::ObjectPath& service_path,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  base::Value* service;
  if (!stub_services_.Get(service_path.value(), &service)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  // Set Associating
  base::StringValue associating_value(flimflam::kStateAssociation);
  SetServiceProperty(service_path.value(),
                     flimflam::kStateProperty,
                     associating_value);
  // Set Online after a delay
  const int kConnectDelaySeconds = 5;
  base::StringValue online_value(flimflam::kStateOnline);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ShillServiceClientStub::SetProperty,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_path,
                 flimflam::kStateProperty,
                 online_value,
                 base::Bind(&base::DoNothing),
                 error_callback),
                 base::TimeDelta::FromSeconds(kConnectDelaySeconds));
  callback.Run();
}

void ShillServiceClientStub::Disconnect(const dbus::ObjectPath& service_path,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  base::Value* service;
  if (!stub_services_.Get(service_path.value(), &service)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  // Set Idle after a delay
  const int kConnectDelaySeconds = 2;
  base::StringValue idle_value(flimflam::kStateIdle);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ShillServiceClientStub::SetProperty,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_path,
                 flimflam::kStateProperty,
                 idle_value,
                 base::Bind(&base::DoNothing),
                 error_callback),
                 base::TimeDelta::FromSeconds(kConnectDelaySeconds));
  callback.Run();
}

void ShillServiceClientStub::Remove(const dbus::ObjectPath& service_path,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillServiceClientStub::ActivateCellularModem(
    const dbus::ObjectPath& service_path,
    const std::string& carrier,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillServiceClientStub::CompleteCellularActivation(
    const dbus::ObjectPath& service_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

bool ShillServiceClientStub::CallActivateCellularModemAndBlock(
    const dbus::ObjectPath& service_path,
    const std::string& carrier) {
  return true;
}

ShillServiceClient::TestInterface* ShillServiceClientStub::GetTestInterface() {
  return this;
}

// ShillServiceClient::TestInterface overrides.

void ShillServiceClientStub::AddService(const std::string& service_path,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::string& state,
                                        bool add_to_watch_list) {
  AddServiceWithIPConfig(service_path, name, type, state, "",
                         add_to_watch_list);
}

void ShillServiceClientStub::AddServiceWithIPConfig(
    const std::string& service_path,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    const std::string& ipconfig_path,
    bool add_to_watch_list) {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddService(service_path, add_to_watch_list);

  base::DictionaryValue* properties =
      GetModifiableServiceProperties(service_path);
  properties->SetWithoutPathExpansion(
      flimflam::kSSIDProperty,
      base::Value::CreateStringValue(service_path));
  properties->SetWithoutPathExpansion(
      flimflam::kNameProperty,
      base::Value::CreateStringValue(name));
  properties->SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(type));
  properties->SetWithoutPathExpansion(
      flimflam::kStateProperty,
      base::Value::CreateStringValue(state));
  if (!ipconfig_path.empty())
    properties->SetWithoutPathExpansion(
        shill::kIPConfigProperty,
        base::Value::CreateStringValue(ipconfig_path));
}

void ShillServiceClientStub::RemoveService(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      RemoveService(service_path);

  stub_services_.RemoveWithoutPathExpansion(service_path, NULL);
}

void ShillServiceClientStub::SetServiceProperty(const std::string& service_path,
                                                const std::string& property,
                                                const base::Value& value) {
  SetProperty(dbus::ObjectPath(service_path), property, value,
              base::Bind(&base::DoNothing),
              base::Bind(&ErrorFunction));
}

const base::DictionaryValue* ShillServiceClientStub::GetServiceProperties(
    const std::string& service_path) const {
  const base::DictionaryValue* properties = NULL;
  stub_services_.GetDictionaryWithoutPathExpansion(service_path, &properties);
  return properties;
}

void ShillServiceClientStub::ClearServices() {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      ClearServices();

  stub_services_.Clear();
}

void ShillServiceClientStub::SetDefaultProperties() {
  const bool add_to_watchlist = true;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableStubEthernet)) {
    AddService("stub_ethernet", "eth0",
               flimflam::kTypeEthernet,
               flimflam::kStateOnline,
               add_to_watchlist);
  }

  AddService("stub_wifi1", "wifi1",
             flimflam::kTypeWifi,
             flimflam::kStateOnline,
             add_to_watchlist);
  SetServiceProperty("stub_wifi1",
                     flimflam::kSecurityProperty,
                     base::StringValue(flimflam::kSecurityWep));

  AddService("stub_wifi2", "wifi2_PSK",
             flimflam::kTypeWifi,
             flimflam::kStateIdle,
             add_to_watchlist);
  SetServiceProperty("stub_wifi2",
                     flimflam::kSecurityProperty,
                     base::StringValue(flimflam::kSecurityPsk));
  base::FundamentalValue strength_value(80);
  SetServiceProperty("stub_wifi2",
                     flimflam::kSignalStrengthProperty,
                     strength_value);

  AddService("stub_cellular1", "cellular1",
             flimflam::kTypeCellular,
             flimflam::kStateIdle,
             add_to_watchlist);
  base::StringValue technology_value(flimflam::kNetworkTechnologyGsm);
  SetServiceProperty("stub_cellular1",
                     flimflam::kNetworkTechnologyProperty,
                     technology_value);
  SetServiceProperty("stub_cellular1",
                     flimflam::kActivationStateProperty,
                     base::StringValue(flimflam::kActivationStateNotActivated));
  SetServiceProperty("stub_cellular1",
                     flimflam::kRoamingStateProperty,
                     base::StringValue(flimflam::kRoamingStateHome));

  AddService("stub_vpn1", "vpn1",
             flimflam::kTypeVPN,
             flimflam::kStateOnline,
             add_to_watchlist);

  AddService("stub_vpn2", "vpn2",
             flimflam::kTypeVPN,
             flimflam::kStateOffline,
             add_to_watchlist);
}

void ShillServiceClientStub::PassStubServiceProperties(
    const dbus::ObjectPath& service_path,
    const DictionaryValueCallback& callback) {
  base::DictionaryValue* dict = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(
      service_path.value(), &dict)) {
    base::DictionaryValue empty_dictionary;
    callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *dict);
}

void ShillServiceClientStub::NotifyObserversPropertyChanged(
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
  FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                    GetObserverList(service_path),
                    OnPropertyChanged(property, *value));
}

base::DictionaryValue* ShillServiceClientStub::GetModifiableServiceProperties(
    const std::string& service_path) {
  base::DictionaryValue* properties = NULL;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(
      service_path, &properties)) {
    properties = new base::DictionaryValue;
    stub_services_.Set(service_path, properties);
  }
  return properties;
}

ShillServiceClientStub::PropertyObserverList&
ShillServiceClientStub::GetObserverList(const dbus::ObjectPath& device_path) {
  std::map<dbus::ObjectPath, PropertyObserverList*>::iterator iter =
      observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = observer_list;
  return *observer_list;
}

}  // namespace chromeos

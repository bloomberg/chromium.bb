// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/shill_service_observer.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kLogModule[] = "ShillPropertyHandler";

// Limit the number of services we observe. Since they are listed in priority
// order, it should be reasonable to ignore services past this.
const size_t kMaxObservedServices = 100;

const base::ListValue* GetListValue(const std::string& key,
                                    const base::Value& value) {
  const base::ListValue* vlist = NULL;
  if (!value.GetAsList(&vlist)) {
    LOG(ERROR) << "Error parsing key as list: " << key;
    return NULL;
  }
  return vlist;
}

}  // namespace

namespace chromeos {
namespace internal {

ShillPropertyHandler::ShillPropertyHandler(Listener* listener)
    : listener_(listener),
      shill_manager_(DBusThreadManager::Get()->GetShillManagerClient()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

ShillPropertyHandler::~ShillPropertyHandler() {
  // Delete network service observers.
  STLDeleteContainerPairSecondPointers(
      observed_networks_.begin(), observed_networks_.end());
  CHECK(shill_manager_ == DBusThreadManager::Get()->GetShillManagerClient());
  shill_manager_->RemovePropertyChangedObserver(this);
}

void ShillPropertyHandler::Init() {
  shill_manager_->GetProperties(
      base::Bind(&ShillPropertyHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  shill_manager_->AddPropertyChangedObserver(this);
}

void ShillPropertyHandler::SetTechnologyEnabled(
    const std::string& technology,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  if (enabled) {
    shill_manager_->EnableTechnology(
        technology,
        base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   kLogModule, technology, error_callback));
  } else {
    shill_manager_->DisableTechnology(
        technology,
        base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   kLogModule, technology, error_callback));
  }
}

void ShillPropertyHandler::RequestScan() const {
  shill_manager_->RequestScan(
      "",
      base::Bind(&base::DoNothing),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, "", network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestProperties(ManagedState::ManagedType type,
                                             const std::string& path) {
  if (pending_updates_[type].find(path) != pending_updates_[type].end())
    return;  // Update already requested.

  pending_updates_[type].insert(path);
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback,
                   weak_ptr_factory_.GetWeakPtr(), type, path));
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback,
                   weak_ptr_factory_.GetWeakPtr(), type, path));
  } else {
    NOTREACHED();
  }
}

void ShillPropertyHandler::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  if (ManagerPropertyChanged(key, value))
    listener_->ManagerPropertyChanged();
}

//------------------------------------------------------------------------------
// Private methods

void ShillPropertyHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Manager properties:" << call_status;
    return;
  }
  bool notify = false;
  bool update_service_list = false;
  for (base::DictionaryValue::Iterator iter(properties);
       iter.HasNext(); iter.Advance()) {
    // Defer updating Services until all other properties have been updated.
    if (iter.key() == flimflam::kServicesProperty)
      update_service_list = true;
    else
      notify |= ManagerPropertyChanged(iter.key(), iter.value());
  }
  // Now update the service list which can safely assume other properties have
  // been initially set.
  if (update_service_list) {
    const base::Value* value = NULL;
    if (properties.GetWithoutPathExpansion(flimflam::kServicesProperty, &value))
      notify |= ManagerPropertyChanged(flimflam::kServicesProperty, *value);
  }
  if (notify)
    listener_->ManagerPropertyChanged();
}

bool ShillPropertyHandler::ManagerPropertyChanged(const std::string& key,
                                                  const base::Value& value) {
  bool notify_manager_changed = false;
  if (key == flimflam::kServicesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateManagedList(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
  } else if (key == flimflam::kServiceWatchListProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateObservedNetworkServices(*vlist);
  } else if (key == flimflam::kDevicesProperty) {
    const ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateManagedList(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
  } else if (key == flimflam::kAvailableTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateAvailableTechnologies(*vlist);
      notify_manager_changed = true;
    }
  } else if (key == flimflam::kEnabledTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateEnabledTechnologies(*vlist);
      notify_manager_changed = true;
    }
  }
  return notify_manager_changed;
}

void ShillPropertyHandler::UpdateManagedList(ManagedState::ManagedType type,
                                             const base::ListValue& entries) {
  listener_->UpdateManagedList(type, entries);
  // Do not send a ManagerPropertyChanged notification to the Listener if
  // RequestProperties has been called (ManagedStateListChanged will be
  // called when the update requests have completed). If no requests
  // have been made, call ManagedStateListChanged to indicate that the
  // order of the list has changed.
  if (pending_updates_[type].size() == 0)
    listener_->ManagedStateListChanged(type);
}

void ShillPropertyHandler::UpdateObservedNetworkServices(
    const base::ListValue& entries) {
  // Watch all networks in the watch list.
  ShillServiceObserverMap new_observed;
  for (base::ListValue::const_iterator iter1 = entries.begin();
       iter1 != entries.end(); ++iter1) {
    std::string path;
    (*iter1)->GetAsString(&path);
    if (path.empty())
      continue;
    ShillServiceObserverMap::iterator iter2 = observed_networks_.find(path);
    if (iter2 != observed_networks_.end()) {
      new_observed[path] = iter2->second;
    } else {
      // Request an update for the network.
      RequestProperties(ManagedState::MANAGED_TYPE_NETWORK, path);
      // Create an observer for future updates.
      new_observed[path] = new ShillServiceObserver(
          path, base::Bind(
              &ShillPropertyHandler::NetworkServicePropertyChangedCallback,
              weak_ptr_factory_.GetWeakPtr()));
      network_event_log::AddEntry(kLogModule, "StartObserving", path);
    }
    observed_networks_.erase(path);
    // Limit the number of observed services.
    if (new_observed.size() >= kMaxObservedServices)
      break;
  }
  // Delete network service observers still in observed_networks_.
  for (ShillServiceObserverMap::iterator iter =  observed_networks_.begin();
       iter != observed_networks_.end(); ++iter) {
    network_event_log::AddEntry(kLogModule, "StopObserving", iter->first);
    delete iter->second;
  }
  observed_networks_.swap(new_observed);
}

void ShillPropertyHandler::GetPropertiesCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  VLOG(2) << "GetPropertiesCallback: " << type << " : " << path;
  pending_updates_[type].erase(path);
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get properties for: " << path
               << ": " << call_status;
    return;
  }
  listener_->UpdateManagedStateProperties(type, path, properties);
  // Notify the listener only when all updates for that type have completed.
  if (pending_updates_[type].size() == 0)
    listener_->ManagedStateListChanged(type);
}

void ShillPropertyHandler::NetworkServicePropertyChangedCallback(
    const std::string& path,
    const std::string& key,
    const base::Value& value) {
  if (key == shill::kIPConfigProperty) {
    // Handle IPConfig here and call listener_->UpdateNetworkServiceIPAddress
    // when the request completes.
    std::string ip_config_path;
    value.GetAsString(&ip_config_path);
    DCHECK(!ip_config_path.empty());
    DBusThreadManager::Get()->GetShillIPConfigClient()->GetProperties(
        dbus::ObjectPath(ip_config_path),
        base::Bind(&ShillPropertyHandler::GetIPConfigCallback,
                   weak_ptr_factory_.GetWeakPtr(), path));
  } else {
    listener_->UpdateNetworkServiceProperty(path, key, value);
  }
}

void ShillPropertyHandler::GetIPConfigCallback(
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties)  {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get IP properties for: " << service_path;
    return;
  }
  std::string ip_address;
  if (!properties.GetStringWithoutPathExpansion(flimflam::kAddressProperty,
                                                &ip_address)) {
    LOG(ERROR) << "Failed to get IP Address property for: " << service_path;
    return;
  }
  listener_->UpdateNetworkServiceIPAddress(service_path, ip_address);
}

}  // namespace internal
}  // namespace chromeos

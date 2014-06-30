// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include <sstream>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Limit the number of services or devices we observe. Since they are listed in
// priority order, it should be reasonable to ignore services past this.
const size_t kMaxObserved = 100;

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

// Class to manage Shill service property changed observers. Observers are
// added on construction and removed on destruction. Runs the handler when
// OnPropertyChanged is called.
class ShillPropertyObserver : public ShillPropertyChangedObserver {
 public:
  typedef base::Callback<void(ManagedState::ManagedType type,
                              const std::string& service,
                              const std::string& name,
                              const base::Value& value)> Handler;

  ShillPropertyObserver(ManagedState::ManagedType type,
                        const std::string& path,
                        const Handler& handler)
      : type_(type),
        path_(path),
        handler_(handler) {
    if (type_ == ManagedState::MANAGED_TYPE_NETWORK) {
      DVLOG(2) << "ShillPropertyObserver: Network: " << path;
      DBusThreadManager::Get()->GetShillServiceClient()->
          AddPropertyChangedObserver(dbus::ObjectPath(path_), this);
    } else if (type_ == ManagedState::MANAGED_TYPE_DEVICE) {
      DVLOG(2) << "ShillPropertyObserver: Device: " << path;
      DBusThreadManager::Get()->GetShillDeviceClient()->
          AddPropertyChangedObserver(dbus::ObjectPath(path_), this);
    } else {
      NOTREACHED();
    }
  }

  virtual ~ShillPropertyObserver() {
    if (type_ == ManagedState::MANAGED_TYPE_NETWORK) {
      DBusThreadManager::Get()->GetShillServiceClient()->
          RemovePropertyChangedObserver(dbus::ObjectPath(path_), this);
    } else if (type_ == ManagedState::MANAGED_TYPE_DEVICE) {
      DBusThreadManager::Get()->GetShillDeviceClient()->
          RemovePropertyChangedObserver(dbus::ObjectPath(path_), this);
    } else {
      NOTREACHED();
    }
  }

  // ShillPropertyChangedObserver overrides.
  virtual void OnPropertyChanged(const std::string& key,
                                 const base::Value& value) OVERRIDE {
    handler_.Run(type_, path_, key, value);
  }

 private:
  ManagedState::ManagedType type_;
  std::string path_;
  Handler handler_;

  DISALLOW_COPY_AND_ASSIGN(ShillPropertyObserver);
};

//------------------------------------------------------------------------------
// ShillPropertyHandler

ShillPropertyHandler::ShillPropertyHandler(Listener* listener)
    : listener_(listener),
      shill_manager_(DBusThreadManager::Get()->GetShillManagerClient()) {
}

ShillPropertyHandler::~ShillPropertyHandler() {
  // Delete network service observers.
  STLDeleteContainerPairSecondPointers(
      observed_networks_.begin(), observed_networks_.end());
  STLDeleteContainerPairSecondPointers(
      observed_devices_.begin(), observed_devices_.end());
  CHECK(shill_manager_ == DBusThreadManager::Get()->GetShillManagerClient());
  shill_manager_->RemovePropertyChangedObserver(this);
}

void ShillPropertyHandler::Init() {
  UpdateManagerProperties();
  shill_manager_->AddPropertyChangedObserver(this);
}

void ShillPropertyHandler::UpdateManagerProperties() {
  NET_LOG_EVENT("UpdateManagerProperties", "");
  shill_manager_->GetProperties(
      base::Bind(&ShillPropertyHandler::ManagerPropertiesCallback,
                 AsWeakPtr()));
}

bool ShillPropertyHandler::IsTechnologyAvailable(
    const std::string& technology) const {
  return available_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyEnabled(
    const std::string& technology) const {
  return enabled_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyEnabling(
    const std::string& technology) const {
  return enabling_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyUninitialized(
    const std::string& technology) const {
  return uninitialized_technologies_.count(technology) != 0;
}

void ShillPropertyHandler::SetTechnologyEnabled(
    const std::string& technology,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  if (enabled) {
    enabling_technologies_.insert(technology);
    shill_manager_->EnableTechnology(
        technology,
        base::Bind(&base::DoNothing),
        base::Bind(&ShillPropertyHandler::EnableTechnologyFailed,
                   AsWeakPtr(), technology, error_callback));
  } else {
    // Immediately clear locally from enabled and enabling lists.
    enabled_technologies_.erase(technology);
    enabling_technologies_.erase(technology);
    shill_manager_->DisableTechnology(
        technology,
        base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "SetTechnologyEnabled Failed",
                   technology, error_callback));
  }
}

void ShillPropertyHandler::SetCheckPortalList(
    const std::string& check_portal_list) {
  base::StringValue value(check_portal_list);
  shill_manager_->SetProperty(
      shill::kCheckPortalListProperty,
      value,
      base::Bind(&base::DoNothing),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "SetCheckPortalList Failed",
                 "", network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestScan() const {
  shill_manager_->RequestScan(
      "",
      base::Bind(&base::DoNothing),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "RequestScan Failed",
                 "", network_handler::ErrorCallback()));
}

void ShillPropertyHandler::ConnectToBestServices() const {
  NET_LOG_EVENT("ConnectToBestServices", "");
  shill_manager_->ConnectToBestServices(
      base::Bind(&base::DoNothing),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "ConnectToBestServices Failed",
                 "", network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestProperties(ManagedState::ManagedType type,
                                             const std::string& path) {
  if (pending_updates_[type].find(path) != pending_updates_[type].end())
    return;  // Update already requested.

  NET_LOG_DEBUG("Request Properties: " + ManagedState::TypeToString(type),
                path);
  pending_updates_[type].insert(path);
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback,
                   AsWeakPtr(), type, path));
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback,
                   AsWeakPtr(), type, path));
  } else {
    NOTREACHED();
  }
}

void ShillPropertyHandler::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  ManagerPropertyChanged(key, value);
  CheckPendingStateListUpdates(key);
}

//------------------------------------------------------------------------------
// Private methods

void ShillPropertyHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    NET_LOG_ERROR("ManagerPropertiesCallback",
                  base::StringPrintf("Failed: %d", call_status));
    return;
  }
  NET_LOG_EVENT("ManagerPropertiesCallback", "Success");
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    ManagerPropertyChanged(iter.key(), iter.value());
  }

  CheckPendingStateListUpdates("");
}

void ShillPropertyHandler::CheckPendingStateListUpdates(
    const std::string& key) {
  // Once there are no pending updates, signal the state list changed callbacks.
  if ((key.empty() || key == shill::kServiceCompleteListProperty) &&
      pending_updates_[ManagedState::MANAGED_TYPE_NETWORK].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_NETWORK);
  }
  if ((key.empty() || key == shill::kDevicesProperty) &&
      pending_updates_[ManagedState::MANAGED_TYPE_DEVICE].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_DEVICE);
  }
}

void ShillPropertyHandler::ManagerPropertyChanged(const std::string& key,
                                                  const base::Value& value) {
  NET_LOG_DEBUG("ManagerPropertyChanged", key);
  if (key == shill::kDefaultServiceProperty) {
    std::string service_path;
    value.GetAsString(&service_path);
    listener_->DefaultNetworkServiceChanged(service_path);
  } else if (key == shill::kServiceCompleteListProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
      UpdateProperties(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
      UpdateObserved(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
    }
  } else if (key == shill::kDevicesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
      UpdateProperties(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
      UpdateObserved(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
    }
  } else if (key == shill::kAvailableTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateAvailableTechnologies(*vlist);
  } else if (key == shill::kEnabledTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateEnabledTechnologies(*vlist);
  } else if (key == shill::kUninitializedTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateUninitializedTechnologies(*vlist);
  } else if (key == shill::kProfilesProperty) {
    listener_->ProfileListChanged();
  } else if (key == shill::kCheckPortalListProperty) {
    std::string check_portal_list;
    if (value.GetAsString(&check_portal_list))
      listener_->CheckPortalListChanged(check_portal_list);
  } else {
    VLOG(2) << "Ignored Manager Property: " << key;
  }
}

void ShillPropertyHandler::UpdateProperties(ManagedState::ManagedType type,
                                            const base::ListValue& entries) {
  std::set<std::string>& requested_updates = requested_updates_[type];
  std::set<std::string> new_requested_updates;
  NET_LOG_DEBUG("UpdateProperties: " + ManagedState::TypeToString(type),
                base::StringPrintf("%" PRIuS, entries.GetSize()));
  for (base::ListValue::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter) {
    std::string path;
    (*iter)->GetAsString(&path);
    if (path.empty())
      continue;

    // We add a special case for devices here to work around an issue in shill
    // that prevents it from sending property changed signals for cellular
    // devices (see crbug.com/321854).
    if (type == ManagedState::MANAGED_TYPE_DEVICE ||
        requested_updates.find(path) == requested_updates.end()) {
      RequestProperties(type, path);
    }
    new_requested_updates.insert(path);
  }
  requested_updates.swap(new_requested_updates);
}

void ShillPropertyHandler::UpdateObserved(ManagedState::ManagedType type,
                                          const base::ListValue& entries) {
  ShillPropertyObserverMap& observer_map =
      (type == ManagedState::MANAGED_TYPE_NETWORK)
      ? observed_networks_ : observed_devices_;
  ShillPropertyObserverMap new_observed;
  for (base::ListValue::const_iterator iter1 = entries.begin();
       iter1 != entries.end(); ++iter1) {
    std::string path;
    (*iter1)->GetAsString(&path);
    if (path.empty())
      continue;
    ShillPropertyObserverMap::iterator iter2 = observer_map.find(path);
    if (iter2 != observer_map.end()) {
      new_observed[path] = iter2->second;
    } else {
      // Create an observer for future updates.
      new_observed[path] = new ShillPropertyObserver(
          type, path, base::Bind(
              &ShillPropertyHandler::PropertyChangedCallback, AsWeakPtr()));
    }
    observer_map.erase(path);
    // Limit the number of observed services.
    if (new_observed.size() >= kMaxObserved)
      break;
  }
  // Delete network service observers still in observer_map.
  for (ShillPropertyObserverMap::iterator iter =  observer_map.begin();
       iter != observer_map.end(); ++iter) {
    delete iter->second;
  }
  observer_map.swap(new_observed);
}

void ShillPropertyHandler::UpdateAvailableTechnologies(
    const base::ListValue& technologies) {
  std::stringstream technologies_str;
  technologies_str << technologies;
  NET_LOG_EVENT("AvailableTechnologies:", technologies_str.str());
  available_technologies_.clear();
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    available_technologies_.insert(technology);
  }
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::UpdateEnabledTechnologies(
    const base::ListValue& technologies) {
  std::stringstream technologies_str;
  technologies_str << technologies;
  NET_LOG_EVENT("EnabledTechnologies:", technologies_str.str());
  enabled_technologies_.clear();
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    enabled_technologies_.insert(technology);
    enabling_technologies_.erase(technology);
  }
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::UpdateUninitializedTechnologies(
    const base::ListValue& technologies) {
  std::stringstream technologies_str;
  technologies_str << technologies;
  NET_LOG_EVENT("UninitializedTechnologies:", technologies_str.str());
  uninitialized_technologies_.clear();
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    uninitialized_technologies_.insert(technology);
  }
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::EnableTechnologyFailed(
    const std::string& technology,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  enabling_technologies_.erase(technology);
  network_handler::ShillErrorCallbackFunction(
      "EnableTechnology Failed",
      technology, error_callback,
      dbus_error_name, dbus_error_message);
}

void ShillPropertyHandler::GetPropertiesCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  NET_LOG_DEBUG("GetPropertiesCallback: " + ManagedState::TypeToString(type),
                path);
  pending_updates_[type].erase(path);
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // The shill service no longer exists.  This can happen when a network
    // has been removed.
    NET_LOG_DEBUG("Failed to get properties",
                  base::StringPrintf("%s: %d", path.c_str(), call_status));
    return;
  }
  listener_->UpdateManagedStateProperties(type, path, properties);

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    // Request IPConfig properties.
    const base::Value* value;
    if (properties.GetWithoutPathExpansion(shill::kIPConfigProperty, &value))
      RequestIPConfig(type, path, *value);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    // Clear and request IPConfig properties for each entry in IPConfigs.
    const base::Value* value;
    if (properties.GetWithoutPathExpansion(shill::kIPConfigsProperty, &value))
      RequestIPConfigsList(type, path, *value);
  }

  // Notify the listener only when all updates for that type have completed.
  if (pending_updates_[type].size() == 0)
    listener_->ManagedStateListChanged(type);
}

void ShillPropertyHandler::PropertyChangedCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& key,
    const base::Value& value) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK &&
      key == shill::kIPConfigProperty) {
    RequestIPConfig(type, path, value);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE &&
      key == shill::kIPConfigsProperty) {
    RequestIPConfigsList(type, path, value);
  }

  if (type == ManagedState::MANAGED_TYPE_NETWORK)
    listener_->UpdateNetworkServiceProperty(path, key, value);
  else if (type == ManagedState::MANAGED_TYPE_DEVICE)
    listener_->UpdateDeviceProperty(path, key, value);
  else
    NOTREACHED();
}

void ShillPropertyHandler::RequestIPConfig(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_path_value) {
  std::string ip_config_path;
  if (!ip_config_path_value.GetAsString(&ip_config_path) ||
      ip_config_path.empty()) {
    NET_LOG_ERROR("Invalid IPConfig", path);
    return;
  }
  DBusThreadManager::Get()->GetShillIPConfigClient()->GetProperties(
      dbus::ObjectPath(ip_config_path),
      base::Bind(&ShillPropertyHandler::GetIPConfigCallback,
                 AsWeakPtr(), type, path, ip_config_path));
}

void ShillPropertyHandler::RequestIPConfigsList(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_list_value) {
  const base::ListValue* ip_configs;
  if (!ip_config_list_value.GetAsList(&ip_configs))
    return;
  for (base::ListValue::const_iterator iter = ip_configs->begin();
       iter != ip_configs->end(); ++iter) {
    RequestIPConfig(type, path, **iter);
  }
}

void ShillPropertyHandler::GetIPConfigCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties)  {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // IP Config properties not availabe. Shill will emit a property change
    // when they are.
    NET_LOG_EVENT(
        base::StringPrintf("Failed to get IP Config properties: %s: %d",
                           ip_config_path.c_str(), call_status), path);
    return;
  }
  NET_LOG_EVENT("IP Config properties received", path);
  listener_->UpdateIPConfigProperties(type, path, ip_config_path, properties);
}

}  // namespace internal
}  // namespace chromeos

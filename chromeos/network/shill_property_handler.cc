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
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kLogModule[] = "ShillPropertyHandler";

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
      DBusThreadManager::Get()->GetShillServiceClient()->
          AddPropertyChangedObserver(dbus::ObjectPath(path_), this);
    } else if (type_ == ManagedState::MANAGED_TYPE_DEVICE) {
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
      shill_manager_(DBusThreadManager::Get()->GetShillManagerClient()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
  shill_manager_->GetProperties(
      base::Bind(&ShillPropertyHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  shill_manager_->AddPropertyChangedObserver(this);
}

bool ShillPropertyHandler::TechnologyAvailable(
    const std::string& technology) const {
  return available_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::TechnologyEnabled(
    const std::string& technology) const {
  return enabled_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::TechnologyUninitialized(
    const std::string& technology) const {
  return uninitialized_technologies_.count(technology) != 0;
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
  // If the service watch or device list changed and there are no pending
  // updates, signal the state list changed callback.
  if ((key == flimflam::kServiceWatchListProperty) &&
      pending_updates_[ManagedState::MANAGED_TYPE_NETWORK].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_NETWORK);
  }
  if (key == flimflam::kDevicesProperty &&
      pending_updates_[ManagedState::MANAGED_TYPE_DEVICE].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_DEVICE);
  }
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
  // If there are no pending updates, signal the state list changed callbacks.
  if (pending_updates_[ManagedState::MANAGED_TYPE_NETWORK].size() == 0)
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_NETWORK);
  if (pending_updates_[ManagedState::MANAGED_TYPE_DEVICE].size() == 0)
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_DEVICE);
}

bool ShillPropertyHandler::ManagerPropertyChanged(const std::string& key,
                                                  const base::Value& value) {
  bool notify_manager_changed = false;
  if (key == flimflam::kServicesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
  } else if (key == flimflam::kServiceWatchListProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      UpdateObserved(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
    }
  } else if (key == flimflam::kDevicesProperty) {
    const ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
      UpdateObserved(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
    }
  } else if (key == flimflam::kAvailableTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      UpdateAvailableTechnologies(*vlist);
      notify_manager_changed = true;
    }
  } else if (key == flimflam::kEnabledTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      UpdateEnabledTechnologies(*vlist);
      notify_manager_changed = true;
    }
  } else if (key == shill::kUninitializedTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      UpdateUninitializedTechnologies(*vlist);
      notify_manager_changed = true;
    }
  }
  return notify_manager_changed;
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
      // Request an update.
      RequestProperties(type, path);
      // Create an observer for future updates.
      new_observed[path] = new ShillPropertyObserver(
          type, path, base::Bind(
              &ShillPropertyHandler::PropertyChangedCallback,
              weak_ptr_factory_.GetWeakPtr()));
      network_event_log::AddEntry(kLogModule, "StartObserving", path);
    }
    observer_map.erase(path);
    // Limit the number of observed services.
    if (new_observed.size() >= kMaxObserved)
      break;
  }
  // Delete network service observers still in observer_map.
  for (ShillPropertyObserverMap::iterator iter =  observer_map.begin();
       iter != observer_map.end(); ++iter) {
    network_event_log::AddEntry(kLogModule, "StopObserving", iter->first);
    delete iter->second;
  }
  observer_map.swap(new_observed);
}

void ShillPropertyHandler::UpdateAvailableTechnologies(
    const base::ListValue& technologies) {
  available_technologies_.clear();
  network_event_log::AddEntry(
      kLogModule, "AvailableTechnologiesChanged",
      base::StringPrintf("Size: %"PRIuS, technologies.GetSize()));
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    available_technologies_.insert(technology);
  }
}

void ShillPropertyHandler::UpdateEnabledTechnologies(
    const base::ListValue& technologies) {
  enabled_technologies_.clear();
  network_event_log::AddEntry(
      kLogModule, "EnabledTechnologiesChanged",
      base::StringPrintf("Size: %"PRIuS, technologies.GetSize()));
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    enabled_technologies_.insert(technology);
  }
}

void ShillPropertyHandler::UpdateUninitializedTechnologies(
    const base::ListValue& technologies) {
  uninitialized_technologies_.clear();
  network_event_log::AddEntry(
      kLogModule, "UninitializedTechnologiesChanged",
      base::StringPrintf("Size: %"PRIuS, technologies.GetSize()));
  for (base::ListValue::const_iterator iter = technologies.begin();
       iter != technologies.end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    DCHECK(!technology.empty());
    uninitialized_technologies_.insert(technology);
  }
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

  if (properties.HasKey(shill::kIPConfigProperty)) {
  // Since this is the first time we received properties for this network,
  // also request its IPConfig parameters.
    std::string ip_config_path;
    if (properties.GetString(shill::kIPConfigProperty, &ip_config_path)) {
      DBusThreadManager::Get()->GetShillIPConfigClient()->GetProperties(
          dbus::ObjectPath(ip_config_path),
          base::Bind(&ShillPropertyHandler::GetIPConfigCallback,
                     weak_ptr_factory_.GetWeakPtr(), path));
    }
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
  if (type == ManagedState::MANAGED_TYPE_NETWORK)
    NetworkServicePropertyChangedCallback(path, key, value);
  else if (type == ManagedState::MANAGED_TYPE_DEVICE)
    NetworkDevicePropertyChangedCallback(path, key, value);
  else
    NOTREACHED();
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

void ShillPropertyHandler::NetworkDevicePropertyChangedCallback(
    const std::string& path,
    const std::string& key,
    const base::Value& value) {
  listener_->UpdateDeviceProperty(path, key, value);
}

}  // namespace internal
}  // namespace chromeos

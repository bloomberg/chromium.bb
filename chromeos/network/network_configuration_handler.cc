// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_handler.h"


#include "base/bind.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Strip surrounding "" from keys (if present).
std::string StripQuotations(const std::string& in_str) {
  size_t len = in_str.length();
  if (len >= 2 && in_str[0] == '"' && in_str[len - 1] == '"')
    return in_str.substr(1, len - 2);
  return in_str;
}

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Config Error: " + error_name;
  NET_LOG(ERROR) << error_msg << ": " << service_path;
  network_handler::RunErrorCallback(error_callback, service_path, error_name,
                                    error_msg);
}

void SetNetworkProfileErrorCallback(
    const std::string& service_path,
    const std::string& profile_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetNetworkProfile Failed: " + profile_path, service_path,
      error_callback, dbus_error_name, dbus_error_message);
}

void LogConfigProperties(const std::string& desc,
                         const std::string& path,
                         const base::DictionaryValue& properties) {
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    std::string v = "******";
    if (shill_property_util::IsLoggableShillProperty(iter.key()))
      base::JSONWriter::Write(iter.value(), &v);
    NET_LOG(USER) << desc << ": " << path + "." + iter.key() + "=" + v;
  }
}

}  // namespace

// Helper class to request from Shill the profile entries associated with a
// Service and delete the service from each profile. Triggers either
// |callback| on success or |error_callback| on failure, and calls
// |handler|->ProfileEntryDeleterCompleted() on completion to delete itself.
class NetworkConfigurationHandler::ProfileEntryDeleter
    : public base::SupportsWeakPtr<ProfileEntryDeleter> {
 public:
  ProfileEntryDeleter(NetworkConfigurationHandler* handler,
                      const std::string& service_path,
                      const std::string& guid,
                      NetworkConfigurationObserver::Source source,
                      const base::Closure& callback,
                      const network_handler::ErrorCallback& error_callback)
      : owner_(handler),
        service_path_(service_path),
        guid_(guid),
        source_(source),
        callback_(callback),
        error_callback_(error_callback) {}

  void Run() {
    DBusThreadManager::Get()
        ->GetShillServiceClient()
        ->GetLoadableProfileEntries(
            dbus::ObjectPath(service_path_),
            base::Bind(&ProfileEntryDeleter::GetProfileEntriesToDeleteCallback,
                       AsWeakPtr()));
  }

 private:
  void GetProfileEntriesToDeleteCallback(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& profile_entries) {
    if (call_status != DBUS_METHOD_CALL_SUCCESS) {
      InvokeErrorCallback(service_path_, error_callback_,
                          "GetLoadableProfileEntriesFailed");
      // ProfileEntryDeleterCompleted will delete this.
      owner_->ProfileEntryDeleterCompleted(service_path_, guid_, source_,
                                           false /* failed */);
      return;
    }

    for (base::DictionaryValue::Iterator iter(profile_entries); !iter.IsAtEnd();
         iter.Advance()) {
      std::string profile_path = StripQuotations(iter.key());
      std::string entry_path;
      iter.value().GetAsString(&entry_path);
      if (profile_path.empty() || entry_path.empty()) {
        NET_LOG(ERROR) << "Failed to parse Profile Entry: " << profile_path
                       << ": " << entry_path;
        continue;
      }
      if (profile_delete_entries_.count(profile_path) != 0) {
        NET_LOG(ERROR) << "Multiple Profile Entries: " << profile_path << ": "
                       << entry_path;
        continue;
      }
      NET_LOG(DEBUG) << "Delete Profile Entry: " << profile_path << ": "
                     << entry_path;
      profile_delete_entries_[profile_path] = entry_path;
      DBusThreadManager::Get()->GetShillProfileClient()->DeleteEntry(
          dbus::ObjectPath(profile_path), entry_path,
          base::Bind(&ProfileEntryDeleter::ProfileEntryDeletedCallback,
                     AsWeakPtr(), profile_path, entry_path),
          base::Bind(&ProfileEntryDeleter::ShillErrorCallback, AsWeakPtr(),
                     profile_path, entry_path));
    }
  }

  void ProfileEntryDeletedCallback(const std::string& profile_path,
                                   const std::string& entry) {
    NET_LOG(DEBUG) << "Profile Entry Deleted: " << profile_path << ": "
                   << entry;
    profile_delete_entries_.erase(profile_path);
    if (!profile_delete_entries_.empty())
      return;
    // Run the callback if this is the last pending deletion.
    if (!callback_.is_null())
      callback_.Run();
    // ProfileEntryDeleterCompleted will delete this.
    owner_->ProfileEntryDeleterCompleted(service_path_, guid_, source_,
                                         true /* success */);
  }

  void ShillErrorCallback(const std::string& profile_path,
                          const std::string& entry,
                          const std::string& dbus_error_name,
                          const std::string& dbus_error_message) {
    // Any Shill Error triggers a failure / error.
    network_handler::ShillErrorCallbackFunction(
        "GetLoadableProfileEntries Failed", profile_path, error_callback_,
        dbus_error_name, dbus_error_message);
    // Delete this even if there are pending deletions; any callbacks will
    // safely become no-ops (by invalidating the WeakPtrs).
    owner_->ProfileEntryDeleterCompleted(service_path_, guid_, source_,
                                         false /* failed */);
  }

  NetworkConfigurationHandler* owner_;  // Unowned
  std::string service_path_;
  std::string guid_;
  NetworkConfigurationObserver::Source source_;
  base::Closure callback_;
  network_handler::ErrorCallback error_callback_;

  // Map of pending profile entry deletions, indexed by profile path.
  std::map<std::string, std::string> profile_delete_entries_;

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryDeleter);
};

// NetworkConfigurationHandler

void NetworkConfigurationHandler::AddObserver(
    NetworkConfigurationObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkConfigurationHandler::RemoveObserver(
    NetworkConfigurationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkConfigurationHandler::GetShillProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG(USER) << "GetShillProperties: " << service_path;
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConfigurationHandler::GetPropertiesCallback,
                 AsWeakPtr(), callback, error_callback, service_path));
}

void NetworkConfigurationHandler::SetShillProperties(
    const std::string& service_path,
    const base::DictionaryValue& shill_properties,
    NetworkConfigurationObserver::Source source,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (shill_properties.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG(USER) << "SetShillProperties: " << service_path;

  scoped_ptr<base::DictionaryValue> properties_to_set(
      shill_properties.DeepCopy());

  // Make sure that the GUID is saved to Shill when setting properties.
  std::string guid;
  properties_to_set->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  if (guid.empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    guid = network_state ? network_state->guid() : base::GenerateGUID();
    properties_to_set->SetStringWithoutPathExpansion(shill::kGuidProperty,
                                                     guid);
  }

  LogConfigProperties("SetProperty", service_path, *properties_to_set);

  scoped_ptr<base::DictionaryValue> properties_copy(
      properties_to_set->DeepCopy());
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperties(
      dbus::ObjectPath(service_path), *properties_to_set,
      base::Bind(&NetworkConfigurationHandler::SetPropertiesSuccessCallback,
                 AsWeakPtr(), service_path, base::Passed(&properties_copy),
                 source, callback),
      base::Bind(&NetworkConfigurationHandler::SetPropertiesErrorCallback,
                 AsWeakPtr(), service_path, error_callback));

  // If we set the StaticIPConfig property, request an IP config refresh
  // after calling SetProperties.
  if (properties_to_set->HasKey(shill::kStaticIPConfigProperty))
    RequestRefreshIPConfigs(service_path);
}

void NetworkConfigurationHandler::ClearShillProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (names.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG(USER) << "ClearShillProperties: " << service_path;
  for (std::vector<std::string>::const_iterator iter = names.begin();
       iter != names.end(); ++iter) {
    NET_LOG(DEBUG) << "ClearProperty: " << service_path << "." << *iter;
  }
  DBusThreadManager::Get()->GetShillServiceClient()->ClearProperties(
      dbus::ObjectPath(service_path), names,
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesSuccessCallback,
                 AsWeakPtr(), service_path, names, callback),
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesErrorCallback,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConfigurationHandler::CreateShillConfiguration(
    const base::DictionaryValue& shill_properties,
    NetworkConfigurationObserver::Source source,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  ShillManagerClient* manager =
      DBusThreadManager::Get()->GetShillManagerClient();
  std::string type;
  shill_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  DCHECK(!type.empty());

  std::string network_id =
      shill_property_util::GetNetworkIdFromProperties(shill_properties);

  if (NetworkTypePattern::Ethernet().MatchesType(type)) {
    InvokeErrorCallback(network_id, error_callback,
                        "ConfigureServiceForProfile: Invalid type: " + type);
    return;
  }

  scoped_ptr<base::DictionaryValue> properties_to_set(
      shill_properties.DeepCopy());

  NET_LOG(USER) << "CreateShillConfiguration: " << type << ": " << network_id;

  std::string profile_path;
  properties_to_set->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile_path);
  DCHECK(!profile_path.empty());

  // Make sure that the GUID is saved to Shill when configuring networks.
  std::string guid;
  properties_to_set->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  if (guid.empty()) {
    guid = base::GenerateGUID();
    properties_to_set->SetStringWithoutPathExpansion(
        ::onc::network_config::kGUID, guid);
  }

  LogConfigProperties("Configure", type, *properties_to_set);

  scoped_ptr<base::DictionaryValue> properties_copy(
      properties_to_set->DeepCopy());
  manager->ConfigureServiceForProfile(
      dbus::ObjectPath(profile_path), *properties_to_set,
      base::Bind(&NetworkConfigurationHandler::RunCreateNetworkCallback,
                 AsWeakPtr(), profile_path, source,
                 base::Passed(&properties_copy), callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "Config.CreateConfiguration Failed", "", error_callback));
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    NetworkConfigurationObserver::Source source,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  // Service.Remove is not reliable. Instead, request the profile entries
  // for the service and remove each entry.
  if (ContainsKey(profile_entry_deleters_, service_path)) {
    InvokeErrorCallback(service_path, error_callback,
                        "RemoveConfigurationInProgress");
    return;
  }

  std::string guid;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (network_state)
    guid = network_state->guid();
  NET_LOG(USER) << "Remove Configuration: " << service_path;
  ProfileEntryDeleter* deleter = new ProfileEntryDeleter(
      this, service_path, guid, source, callback, error_callback);
  profile_entry_deleters_[service_path] = deleter;
  deleter->Run();
}

void NetworkConfigurationHandler::SetNetworkProfile(
    const std::string& service_path,
    const std::string& profile_path,
    NetworkConfigurationObserver::Source source,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG(USER) << "SetNetworkProfile: " << service_path << ": "
                << profile_path;
  base::StringValue profile_path_value(profile_path);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(service_path), shill::kProfileProperty,
      profile_path_value,
      base::Bind(&NetworkConfigurationHandler::SetNetworkProfileCompleted,
                 AsWeakPtr(), service_path, profile_path, source, callback),
      base::Bind(&SetNetworkProfileErrorCallback, service_path, profile_path,
                 error_callback));
}

// NetworkConfigurationHandler Private methods

NetworkConfigurationHandler::NetworkConfigurationHandler()
    : network_state_handler_(NULL) {
}

NetworkConfigurationHandler::~NetworkConfigurationHandler() {
  STLDeleteContainerPairSecondPointers(profile_entry_deleters_.begin(),
                                       profile_entry_deleters_.end());
}

void NetworkConfigurationHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;
}

void NetworkConfigurationHandler::RunCreateNetworkCallback(
    const std::string& profile_path,
    NetworkConfigurationObserver::Source source,
    scoped_ptr<base::DictionaryValue> configure_properties,
    const network_handler::StringResultCallback& callback,
    const dbus::ObjectPath& service_path) {
  if (!callback.is_null())
    callback.Run(service_path.value());
  FOR_EACH_OBSERVER(NetworkConfigurationObserver, observers_,
                    OnConfigurationCreated(service_path.value(), profile_path,
                                           *configure_properties, source));
  // This may also get called when CreateConfiguration is used to update an
  // existing configuration, so request a service update just in case.
  // TODO(pneubeck): Separate 'Create' and 'Update' calls and only trigger
  // this on an update.
  network_state_handler_->RequestUpdateForNetwork(service_path.value());
}

void NetworkConfigurationHandler::ProfileEntryDeleterCompleted(
    const std::string& service_path,
    const std::string& guid,
    NetworkConfigurationObserver::Source source,
    bool success) {
  if (success) {
    FOR_EACH_OBSERVER(NetworkConfigurationObserver, observers_,
                      OnConfigurationRemoved(service_path, guid, source));
  }
  std::map<std::string, ProfileEntryDeleter*>::iterator iter =
      profile_entry_deleters_.find(service_path);
  DCHECK(iter != profile_entry_deleters_.end());
  delete iter->second;
  profile_entry_deleters_.erase(iter);
}

void NetworkConfigurationHandler::SetNetworkProfileCompleted(
    const std::string& service_path,
    const std::string& profile_path,
    NetworkConfigurationObserver::Source source,
    const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
  FOR_EACH_OBSERVER(
      NetworkConfigurationObserver, observers_,
      OnConfigurationProfileChanged(service_path, profile_path, source));
}

void NetworkConfigurationHandler::GetPropertiesCallback(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // Because network services are added and removed frequently, we will see
    // failures regularly, so don't log these.
    network_handler::RunErrorCallback(error_callback, service_path,
                                      network_handler::kDBusFailedError,
                                      network_handler::kDBusFailedErrorMessage);
    return;
  }
  if (callback.is_null())
    return;

  // Get the correct name from WifiHex if necessary.
  scoped_ptr<base::DictionaryValue> properties_copy(properties.DeepCopy());
  std::string name =
      shill_property_util::GetNameFromProperties(service_path, properties);
  if (!name.empty())
    properties_copy->SetStringWithoutPathExpansion(shill::kNameProperty, name);

  // Get the GUID property from NetworkState if it is not set in Shill.
  std::string guid;
  properties.GetStringWithoutPathExpansion(::onc::network_config::kGUID, &guid);
  if (guid.empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    if (network_state) {
      properties_copy->SetStringWithoutPathExpansion(
          ::onc::network_config::kGUID, network_state->guid());
    }
  }

  callback.Run(service_path, *properties_copy.get());
}

void NetworkConfigurationHandler::SetPropertiesSuccessCallback(
    const std::string& service_path,
    scoped_ptr<base::DictionaryValue> set_properties,
    NetworkConfigurationObserver::Source source,
    const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state)
    return;  // Network no longer exists, do not notify or request update.

  FOR_EACH_OBSERVER(NetworkConfigurationObserver, observers_,
                    OnPropertiesSet(service_path, network_state->guid(),
                                    *set_properties, source));
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::SetPropertiesErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetProperties Failed", service_path, error_callback,
      dbus_error_name, dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesSuccessCallback(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const base::ListValue& result) {
  const std::string kClearPropertiesFailedError("Error.ClearPropertiesFailed");
  DCHECK(names.size() == result.GetSize())
      << "Incorrect result size from ClearProperties.";

  for (size_t i = 0; i < result.GetSize(); ++i) {
    bool success = false;
    result.GetBoolean(i, &success);
    if (!success) {
      // If a property was cleared that has never been set, the clear will fail.
      // We do not track which properties have been set, so just log the error.
      NET_LOG(ERROR) << "ClearProperties Failed: " << service_path << ": "
                     << names[i];
    }
  }

  if (!callback.is_null())
    callback.Run();
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.ClearProperties Failed", service_path, error_callback,
      dbus_error_name, dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::RequestRefreshIPConfigs(
    const std::string& service_path) {
  if (!network_device_handler_)
    return;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state || network_state->device_path().empty())
    return;
  network_device_handler_->RequestRefreshIPConfigs(
      network_state->device_path(), base::Bind(&base::DoNothing),
      network_handler::ErrorCallback());
}

// static
NetworkConfigurationHandler* NetworkConfigurationHandler::InitializeForTest(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  NetworkConfigurationHandler* handler = new NetworkConfigurationHandler();
  handler->Init(network_state_handler, network_device_handler);
  return handler;
}

}  // namespace chromeos

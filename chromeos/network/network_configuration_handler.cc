// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Strip surrounding "" from keys (if present).
std::string StripQuotations(const std::string& in_str) {
  size_t len = in_str.length();
  if (len >= 2 && in_str[0] == '"' && in_str[len-1] == '"')
    return in_str.substr(1, len-2);
  return in_str;
}

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Config Error: " + error_name;
  NET_LOG_ERROR(error_msg, service_path);
  network_handler::RunErrorCallback(
      error_callback, service_path, error_name, error_msg);
}

void GetPropertiesCallback(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // Because network services are added and removed frequently, we will see
    // failures regularly, so don't log these.
    network_handler::RunErrorCallback(error_callback,
                                      service_path,
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
  callback.Run(service_path, *properties_copy.get());
}

void SetNetworkProfileErrorCallback(
    const std::string& service_path,
    const std::string& profile_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetNetworkProfile Failed: " + profile_path,
      service_path, error_callback,
      dbus_error_name, dbus_error_message);
}

void LogConfigProperties(const std::string& desc,
                         const std::string& path,
                         const base::DictionaryValue& properties) {
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string v = "******";
    if (!shill_property_util::IsPassphraseKey(iter.key()))
      base::JSONWriter::Write(&iter.value(), &v);
    NET_LOG_DEBUG(desc,  path + "." + iter.key() + "=" + v);
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
                      const base::Closure& callback,
                      const network_handler::ErrorCallback& error_callback)
      : owner_(handler),
        service_path_(service_path),
        callback_(callback),
        error_callback_(error_callback) {
  }

  void Run() {
    DBusThreadManager::Get()->GetShillServiceClient()->
        GetLoadableProfileEntries(
            dbus::ObjectPath(service_path_),
            base::Bind(&ProfileEntryDeleter::GetProfileEntriesToDeleteCallback,
                       AsWeakPtr()));
  }

 private:
  void GetProfileEntriesToDeleteCallback(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& profile_entries) {
    if (call_status != DBUS_METHOD_CALL_SUCCESS) {
      InvokeErrorCallback(
          service_path_, error_callback_, "GetLoadableProfileEntriesFailed");
      owner_->ProfileEntryDeleterCompleted(service_path_);  // Deletes this.
      return;
    }

    for (base::DictionaryValue::Iterator iter(profile_entries);
         !iter.IsAtEnd(); iter.Advance()) {
      std::string profile_path = StripQuotations(iter.key());
      std::string entry_path;
      iter.value().GetAsString(&entry_path);
      if (profile_path.empty() || entry_path.empty()) {
        NET_LOG_ERROR("Failed to parse Profile Entry", base::StringPrintf(
            "%s: %s", profile_path.c_str(), entry_path.c_str()));
        continue;
      }
      if (profile_delete_entries_.count(profile_path) != 0) {
        NET_LOG_ERROR("Multiple Profile Entries", base::StringPrintf(
            "%s: %s", profile_path.c_str(), entry_path.c_str()));
        continue;
      }
      NET_LOG_DEBUG("Delete Profile Entry", base::StringPrintf(
          "%s: %s", profile_path.c_str(), entry_path.c_str()));
      profile_delete_entries_[profile_path] = entry_path;
      DBusThreadManager::Get()->GetShillProfileClient()->DeleteEntry(
          dbus::ObjectPath(profile_path),
          entry_path,
          base::Bind(&ProfileEntryDeleter::ProfileEntryDeletedCallback,
                     AsWeakPtr(), profile_path, entry_path),
          base::Bind(&ProfileEntryDeleter::ShillErrorCallback,
                     AsWeakPtr(), profile_path, entry_path));
    }
  }

  void ProfileEntryDeletedCallback(const std::string& profile_path,
                                   const std::string& entry) {
    NET_LOG_DEBUG("Profile Entry Deleted", base::StringPrintf(
        "%s: %s", profile_path.c_str(), entry.c_str()));
    profile_delete_entries_.erase(profile_path);
    if (!profile_delete_entries_.empty())
      return;
    // Run the callback if this is the last pending deletion.
    if (!callback_.is_null())
      callback_.Run();
    owner_->ProfileEntryDeleterCompleted(service_path_);  // Deletes this.
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
    owner_->ProfileEntryDeleterCompleted(service_path_);  // Deletes this.
  }

  NetworkConfigurationHandler* owner_;  // Unowned
  std::string service_path_;
  base::Closure callback_;
  network_handler::ErrorCallback error_callback_;

  // Map of pending profile entry deletions, indexed by profile path.
  std::map<std::string, std::string> profile_delete_entries_;

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryDeleter);
};

// NetworkConfigurationHandler

void NetworkConfigurationHandler::GetProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NET_LOG_USER("GetProperties", service_path);
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&GetPropertiesCallback,
                 callback, error_callback, service_path));
}

void NetworkConfigurationHandler::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (properties.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG_USER("SetProperties", service_path);
  LogConfigProperties("SetProperty", service_path, properties);

  DBusThreadManager::Get()->GetShillServiceClient()->SetProperties(
      dbus::ObjectPath(service_path),
      properties,
      base::Bind(&NetworkConfigurationHandler::SetPropertiesSuccessCallback,
                 AsWeakPtr(), service_path, callback),
      base::Bind(&NetworkConfigurationHandler::SetPropertiesErrorCallback,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConfigurationHandler::ClearProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (names.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG_USER("ClearProperties", service_path);
  for (std::vector<std::string>::const_iterator iter = names.begin();
       iter != names.end(); ++iter) {
    NET_LOG_DEBUG("ClearProperty", service_path + "." + *iter);
  }
  DBusThreadManager::Get()->GetShillServiceClient()->ClearProperties(
      dbus::ObjectPath(service_path),
      names,
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesSuccessCallback,
                 AsWeakPtr(), service_path, names, callback),
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesErrorCallback,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConfigurationHandler::CreateConfiguration(
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  ShillManagerClient* manager =
      DBusThreadManager::Get()->GetShillManagerClient();
  std::string type;
  properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  DCHECK(!type.empty());
  if (NetworkTypePattern::Ethernet().MatchesType(type)) {
    InvokeErrorCallback(
        shill_property_util::GetNetworkIdFromProperties(properties),
        error_callback,
        "ConfigureServiceForProfile: Invalid type: " + type);
    return;
  }

  NET_LOG_USER("CreateConfiguration: " + type,
               shill_property_util::GetNetworkIdFromProperties(properties));
  LogConfigProperties("Configure", type, properties);

  std::string profile;
  properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                           &profile);
  DCHECK(!profile.empty());
  manager->ConfigureServiceForProfile(
      dbus::ObjectPath(profile),
      properties,
      base::Bind(&NetworkConfigurationHandler::RunCreateNetworkCallback,
                 AsWeakPtr(),
                 callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "Config.CreateConfiguration Failed",
                 "",
                 error_callback));
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  // Service.Remove is not reliable. Instead, request the profile entries
  // for the service and remove each entry.
  if (ContainsKey(profile_entry_deleters_, service_path)) {
    InvokeErrorCallback(
        service_path, error_callback, "RemoveConfigurationInProgress");
    return;
  }

  NET_LOG_USER("Remove Configuration", service_path);
  ProfileEntryDeleter* deleter =
      new ProfileEntryDeleter(this, service_path, callback, error_callback);
  profile_entry_deleters_[service_path] = deleter;
  deleter->Run();
}

void NetworkConfigurationHandler::SetNetworkProfile(
    const std::string& service_path,
    const std::string& profile_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("SetNetworkProfile", service_path + ": " + profile_path);
  base::StringValue profile_path_value(profile_path);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(service_path),
      shill::kProfileProperty,
      profile_path_value,
      callback,
      base::Bind(&SetNetworkProfileErrorCallback,
                 service_path, profile_path, error_callback));
}

// NetworkConfigurationHandler Private methods

NetworkConfigurationHandler::NetworkConfigurationHandler()
    : network_state_handler_(NULL) {
}

NetworkConfigurationHandler::~NetworkConfigurationHandler() {
  STLDeleteContainerPairSecondPointers(
      profile_entry_deleters_.begin(), profile_entry_deleters_.end());
}

void NetworkConfigurationHandler::Init(
    NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
}

void NetworkConfigurationHandler::RunCreateNetworkCallback(
    const network_handler::StringResultCallback& callback,
    const dbus::ObjectPath& service_path) {
  if (!callback.is_null())
    callback.Run(service_path.value());
  // This may also get called when CreateConfiguration is used to update an
  // existing configuration, so request a service update just in case.
  // TODO(pneubeck): Separate 'Create' and 'Update' calls and only trigger
  // this on an update.
  network_state_handler_->RequestUpdateForNetwork(service_path.value());
}

void NetworkConfigurationHandler::ProfileEntryDeleterCompleted(
    const std::string& service_path) {
  std::map<std::string, ProfileEntryDeleter*>::iterator iter =
      profile_entry_deleters_.find(service_path);
  DCHECK(iter != profile_entry_deleters_.end());
  delete iter->second;
  profile_entry_deleters_.erase(iter);
}

void NetworkConfigurationHandler::SetPropertiesSuccessCallback(
    const std::string& service_path,
    const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::SetPropertiesErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetProperties Failed",
      service_path, error_callback,
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
      NET_LOG_ERROR("ClearProperties Failed: " + names[i], service_path);
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
      "Config.ClearProperties Failed",
      service_path, error_callback,
      dbus_error_name, dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

// static
NetworkConfigurationHandler* NetworkConfigurationHandler::InitializeForTest(
    NetworkStateHandler* network_state_handler) {
  NetworkConfigurationHandler* handler = new NetworkConfigurationHandler();
  handler->Init(network_state_handler);
  return handler;
}

}  // namespace chromeos

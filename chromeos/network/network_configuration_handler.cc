// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
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
#include "chromeos/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// None of these error messages are user-facing: they should only appear in
// logs.
const char kErrorsListTag[] = "errors";
const char kClearPropertiesFailedError[] = "Error.ClearPropertiesFailed";
const char kClearPropertiesFailedErrorMessage[] = "Clear properties failed";
const char kDBusFailedError[] = "Error.DBusFailed";
const char kDBusFailedErrorMessage[] = "DBus call failed.";

void ClearPropertiesCallback(
    const std::vector<std::string>& names,
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback,
    const base::ListValue& result) {
  bool some_failed = false;
  for (size_t i = 0; i < result.GetSize(); ++i) {
    bool success;
    if (result.GetBoolean(i, &success)) {
      if (!success) {
        some_failed = true;
        break;
      }
    } else {
      NOTREACHED() << "Result garbled from ClearProperties";
    }
  }

  if (some_failed) {
    DCHECK(names.size() == result.GetSize())
        << "Result wrong size from ClearProperties.";
    scoped_ptr<base::DictionaryValue> error_data(
        network_handler::CreateErrorData(service_path,
                                         kClearPropertiesFailedError,
                                         kClearPropertiesFailedErrorMessage));
    LOG(ERROR) << "ClearPropertiesCallback failed for service path: "
               << service_path;
    error_data->Set("errors", result.DeepCopy());
    scoped_ptr<base::ListValue> name_list(new base::ListValue);
    name_list->AppendStrings(names);
    error_data->Set("names", name_list.release());
    error_callback.Run(kClearPropertiesFailedError, error_data.Pass());
  } else {
    callback.Run();
  }
}

// Used to translate the dbus dictionary callback into one that calls
// the error callback if we have a failure.
void RunCallbackWithDictionaryValue(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& value) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    scoped_ptr<base::DictionaryValue> error_data(
        network_handler::CreateErrorData(service_path,
                                         kDBusFailedError,
                                         kDBusFailedErrorMessage));
    LOG(ERROR) << "CallbackWithDictionaryValue failed for service path: "
               << service_path;
    error_callback.Run(kDBusFailedError, error_data.Pass());
  } else {
    callback.Run(service_path, value);
  }
}

void IgnoreObjectPathCallback(const base::Closure& callback,
                              const dbus::ObjectPath& object_path) {
  callback.Run();
}

// Strip surrounding "" from keys (if present).
std::string StripQuotations(const std::string& in_str) {
  size_t len = in_str.length();
  if (len >= 2 && in_str[0] == '"' && in_str[len-1] == '"')
    return in_str.substr(1, len-2);
  return in_str;
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
      network_handler::ShillErrorCallbackFunction(
          service_path_, error_callback_, "GetLoadableProfileEntries Failed",
          "Failed to get Profile Entries for " + service_path_);
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
    // TODO(stevenjb): Request NetworkStateHandler manager update to update
    // ServiceCompleteList once FavoriteStates are implemented.
    owner_->ProfileEntryDeleterCompleted(service_path_);  // Deletes this.
  }

  void ShillErrorCallback(const std::string& profile_path,
                          const std::string& entry,
                          const std::string& dbus_error_name,
                          const std::string& dbus_error_message) {
    // Any Shill Error triggers a failure / error.
    network_handler::ShillErrorCallbackFunction(
        profile_path, error_callback_, "GetLoadableProfileEntries Shill Error",
        base::StringPrintf("Shill Error for Entry: %s", entry.c_str()));
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
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&RunCallbackWithDictionaryValue,
                 callback,
                 error_callback,
                 service_path));
}

void NetworkConfigurationHandler::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillManagerClient()->ConfigureService(
      properties,
      base::Bind(&IgnoreObjectPathCallback, callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 service_path, error_callback));
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillServiceClient()->ClearProperties(
      dbus::ObjectPath(service_path),
      names,
      base::Bind(&ClearPropertiesCallback,
                 names,
                 service_path,
                 callback,
                 error_callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 service_path, error_callback));
}

void NetworkConfigurationHandler::CreateConfiguration(
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  ShillManagerClient* manager =
      DBusThreadManager::Get()->GetShillManagerClient();

  std::string type;
  properties.GetStringWithoutPathExpansion(flimflam::kTypeProperty, &type);
  // Shill supports ConfigureServiceForProfile only for network type WiFi. In
  // all other cases, we have to rely on GetService for now. This is
  // unproblematic for VPN (user profile only), but will lead to inconsistencies
  // with WiMax, for example.
  if (type == flimflam::kTypeWifi) {
    std::string profile;
    properties.GetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                             &profile);
    manager->ConfigureServiceForProfile(
        dbus::ObjectPath(profile),
        properties,
        base::Bind(&NetworkConfigurationHandler::RunCreateNetworkCallback,
                   AsWeakPtr(), callback),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "", error_callback));
  } else {
    manager->GetService(
        properties,
        base::Bind(&NetworkConfigurationHandler::RunCreateNetworkCallback,
                   AsWeakPtr(), callback),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "", error_callback));
  }
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  // Service.Remove is not reliable. Instead, request the profile entries
  // for the service and remove each entry.
  if (profile_entry_deleters_.count(service_path)) {
    network_handler::ShillErrorCallbackFunction(
        service_path, error_callback, "RemoveConfiguration",
        base::StringPrintf("RemoveConfiguration In-Progress for %s",
                           service_path.c_str()));
    return;
  }
  NET_LOG_USER("Remove Configuration", service_path);
  ProfileEntryDeleter* deleter =
      new ProfileEntryDeleter(this, service_path, callback, error_callback);
  profile_entry_deleters_[service_path] = deleter;
  deleter->Run();
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

// static
NetworkConfigurationHandler* NetworkConfigurationHandler::InitializeForTest(
    NetworkStateHandler* network_state_handler) {
  NetworkConfigurationHandler* handler = new NetworkConfigurationHandler();
  handler->Init(network_state_handler);
  return handler;
}

}  // namespace chromeos

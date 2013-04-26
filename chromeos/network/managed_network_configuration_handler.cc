// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

ManagedNetworkConfigurationHandler* g_configuration_handler_instance = NULL;

const char kLogModule[] = "ManagedNetworkConfigurationHandler";

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kInvalidUserSettingsMessage[] = "User settings are invalid.";
const char kInvalidUserSettings[] = "Error.InvalidUserSettings";
const char kNetworkAlreadyConfiguredMessage[] =
    "Network is already configured.";
const char kNetworkAlreadyConfigured[] = "Error.NetworkAlreadyConfigured";
const char kPoliciesNotInitializedMessage[] = "Policies not initialized.";
const char kPoliciesNotInitialized[] = "Error.PoliciesNotInitialized";
const char kServicePath[] = "servicePath";
const char kSetOnUnconfiguredNetworkMessage[] =
    "Unable to modify properties of an unconfigured network.";
const char kSetOnUnconfiguredNetwork[] = "Error.SetCalledOnUnconfiguredNetwork";
const char kUIDataErrorMessage[] = "UI data contains errors.";
const char kUIDataError[] = "Error.UIData";
const char kUnknownServicePathMessage[] = "Service path is unknown.";
const char kUnknownServicePath[] = "Error.UnknownServicePath";

enum ProfileType {
  PROFILE_NONE,    // Not in any profile.
  PROFILE_SHARED,  // In the shared profile, shared by all users on device.
  PROFILE_USER     // In the user profile, not visible to other users.
};

const char kSharedProfilePath[] = "/profile/default";
const char kUserProfilePath[] = "/profile/chronos/shill";

// This fake credential contains a random postfix which is extremly unlikely to
// be used by any user.
const char kFakeCredential[] = "FAKE_CREDENTIAL_VPaJDV9x";

void RunErrorCallback(const std::string& service_path,
                      const std::string& error_name,
                      const std::string& error_message,
                      const network_handler::ErrorCallback& error_callback) {
  network_event_log::AddEntry(kLogModule, error_name, error_message);
  error_callback.Run(
      error_name,
      make_scoped_ptr(
          network_handler::CreateErrorData(service_path,
                                           error_name,
                                           error_message)));
}

// Returns the NetworkUIData parsed from the UIData property of
// |shill_dictionary|. If parsing fails or the field doesn't exist, returns
// NULL.
scoped_ptr<NetworkUIData> GetUIData(
    const base::DictionaryValue& shill_dictionary) {
  std::string ui_data_blob;
  if (shill_dictionary.GetStringWithoutPathExpansion(
          flimflam::kUIDataProperty,
          &ui_data_blob) &&
      !ui_data_blob.empty()) {
    scoped_ptr<base::DictionaryValue> ui_data_dict =
        onc::ReadDictionaryFromJson(ui_data_blob);
    if (ui_data_dict)
      return make_scoped_ptr(new NetworkUIData(*ui_data_dict));
    else
      LOG(ERROR) << "UIData is not a valid JSON dictionary.";
  }
  return scoped_ptr<NetworkUIData>();
}

// Sets the UIData property in |shill_dictionary| to the serialization of
// |ui_data|.
void SetUIData(const NetworkUIData& ui_data,
               base::DictionaryValue* shill_dictionary) {
  base::DictionaryValue ui_data_dict;
  ui_data.FillDictionary(&ui_data_dict);
  std::string ui_data_blob;
  base::JSONWriter::Write(&ui_data_dict, &ui_data_blob);
  shill_dictionary->SetStringWithoutPathExpansion(flimflam::kUIDataProperty,
                                                  ui_data_blob);
}

// A dummy callback to ignore the result of Shill calls.
void IgnoreString(const std::string& str) {
}

void LogErrorWithDict(const tracked_objects::Location& from_where,
                      const std::string& error_name,
                      const scoped_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << from_where.ToString() << ": " << error_name;
}

void LogErrorMessage(const tracked_objects::Location& from_where,
                     const std::string& error_name,
                     const std::string& error_message) {
  LOG(ERROR) << from_where.ToString() << ": " << error_message;
}

// Removes all kFakeCredential values from sensitive fields (determined by
// onc::FieldIsCredential) of |onc_object|.
void RemoveFakeCredentials(
    const onc::OncValueSignature& signature,
    base::DictionaryValue* onc_object) {
  base::DictionaryValue::Iterator it(*onc_object);
  while (!it.IsAtEnd()) {
    base::Value* value = NULL;
    std::string field_name = it.key();
    // We need the non-const entry to remove nested values but DictionaryValue
    // has no non-const iterator.
    onc_object->GetWithoutPathExpansion(field_name, &value);
    // Advance before delete.
    it.Advance();

    // If |value| is a dictionary, recurse.
    base::DictionaryValue* nested_object = NULL;
    if (value->GetAsDictionary(&nested_object)) {
      const onc::OncFieldSignature* field_signature =
          onc::GetFieldSignature(signature, field_name);

      RemoveFakeCredentials(*field_signature->value_signature,
                            nested_object);
      continue;
    }

    // If |value| is a string, check if it is a fake credential.
    std::string string_value;
    if (value->GetAsString(&string_value) &&
        onc::FieldIsCredential(signature, field_name)) {
      if (string_value == kFakeCredential) {
        // The value wasn't modified by the UI, thus we remove the field to keep
        // the existing value that is stored in Shill.
        onc_object->RemoveWithoutPathExpansion(field_name, NULL);
      }
      // Otherwise, the value is set and modified by the UI, thus we keep that
      // value to overwrite whatever is stored in Shill.
    }
  }
}

// Creates a Shill property dictionary from the given arguments. The resulting
// dictionary will be sent to Shill by the caller. Depending on the profile
// path, |policy| is interpreted as the user or device policy and |settings| as
// the user or shared settings.
scoped_ptr<base::DictionaryValue> CreateShillConfiguration(
    const std::string& profile_path,
    const std::string& guid,
    const base::DictionaryValue* policy,
    const base::DictionaryValue* settings) {
  scoped_ptr<base::DictionaryValue> effective;

  onc::ONCSource onc_source;
  if (policy) {
    if (profile_path == kSharedProfilePath) {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          NULL,  // no user policy
          policy,  // device policy
          NULL,  // no user settings
          settings);  // shared settings
      onc_source = onc::ONC_SOURCE_DEVICE_POLICY;
    } else {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          policy,  // user policy
          NULL,  // no device policy
          settings,  // user settings
          NULL);  // no shared settings
      onc_source = onc::ONC_SOURCE_USER_POLICY;
    }
  } else if (settings) {
    effective.reset(settings->DeepCopy());
    // TODO(pneubeck): change to source ONC_SOURCE_USER
    onc_source = onc::ONC_SOURCE_NONE;
  } else {
    NOTREACHED();
    onc_source = onc::ONC_SOURCE_NONE;
  }

  RemoveFakeCredentials(onc::kNetworkConfigurationSignature,
                        effective.get());

  effective->SetStringWithoutPathExpansion(onc::network_config::kGUID, guid);

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                     *effective));

  shill_dictionary->SetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                                  profile_path);

  scoped_ptr<NetworkUIData> ui_data;
  if (policy)
    ui_data = CreateUIDataFromONC(onc_source, *policy);
  else
    ui_data.reset(new NetworkUIData());

  if (settings) {
    // Shill doesn't know that sensitive data is contained in the UIData
    // property and might write it into logs or other insecure places. Thus, we
    // have to remove or mask credentials.
    //
    // Shill's GetProperties doesn't return credentials. Masking credentials
    // instead of just removing them, allows remembering if a credential is set
    // or not.
    scoped_ptr<base::DictionaryValue> sanitized_settings(
        onc::MaskCredentialsInOncObject(onc::kNetworkConfigurationSignature,
                                        *settings,
                                        kFakeCredential));
    ui_data->set_user_settings(sanitized_settings.Pass());
  }

  SetUIData(*ui_data, shill_dictionary.get());

  VLOG(2) << "Created Shill properties: " << *shill_dictionary;

  return shill_dictionary.Pass();
}

// Returns true if |policy| matches |onc_network_part|. This is should be the
// only such matching function within Chrome. Shill does such matching in
// several functions for network identification. For compatibility, we currently
// should stick to Shill's matching behavior.
bool IsPolicyMatching(const base::DictionaryValue& policy,
                      const base::DictionaryValue& onc_network_part) {
  std::string policy_type;
  policy.GetStringWithoutPathExpansion(onc::network_config::kType,
                                       &policy_type);
  std::string network_type;
  onc_network_part.GetStringWithoutPathExpansion(onc::network_config::kType,
                                                 &network_type);
  if (policy_type != network_type)
    return false;

  if (network_type != onc::network_type::kWiFi)
    return false;

  std::string policy_ssid;
  policy.GetStringWithoutPathExpansion(onc::wifi::kSSID, &policy_ssid);
  std::string network_ssid;
  onc_network_part.GetStringWithoutPathExpansion(onc::wifi::kSSID,
                                                 &network_ssid);
  return (policy_ssid == network_ssid);
}

// Returns the policy of |policies| matching |onc_network_part|, if any
// exists. Returns NULL otherwise.
const base::DictionaryValue* FindMatchingPolicy(
    const ManagedNetworkConfigurationHandler::PolicyMap &policies,
    const base::DictionaryValue& onc_network_part) {
  for (ManagedNetworkConfigurationHandler::PolicyMap::const_iterator it =
           policies.begin(); it != policies.end(); ++it) {
    if (IsPolicyMatching(*it->second, onc_network_part))
      return it->second;
  }
  return NULL;
}

void TranslatePropertiesToOncAndRunCallback(
    const network_handler::DictionaryResultCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  scoped_ptr<base::DictionaryValue> onc_network(
      onc::TranslateShillServiceToONCPart(
          shill_properties,
          &onc::kNetworkWithStateSignature));
  callback.Run(service_path, *onc_network);
}

}  // namespace

// static
void ManagedNetworkConfigurationHandler::Initialize() {
  CHECK(!g_configuration_handler_instance);
  g_configuration_handler_instance = new ManagedNetworkConfigurationHandler;
}

// static
bool ManagedNetworkConfigurationHandler::IsInitialized() {
  return g_configuration_handler_instance;
}

// static
void ManagedNetworkConfigurationHandler::Shutdown() {
  CHECK(g_configuration_handler_instance);
  delete g_configuration_handler_instance;
  g_configuration_handler_instance = NULL;
}

// static
ManagedNetworkConfigurationHandler* ManagedNetworkConfigurationHandler::Get() {
  CHECK(g_configuration_handler_instance);
  return g_configuration_handler_instance;
}

void ManagedNetworkConfigurationHandler::GetManagedProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (!user_policies_initialized_ || !device_policies_initialized_) {
    RunErrorCallback(service_path,
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }
  NetworkConfigurationHandler::Get()->GetProperties(
      service_path,
      base::Bind(
          &ManagedNetworkConfigurationHandler::GetManagedPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(),
          callback,
          error_callback),
      error_callback);
}

void ManagedNetworkConfigurationHandler::GetManagedPropertiesCallback(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string profile_path;
  ProfileType profile_type = PROFILE_NONE;
  if (shill_properties.GetStringWithoutPathExpansion(
          flimflam::kProfileProperty, &profile_path)) {
    if (profile_path == kSharedProfilePath)
      profile_type = PROFILE_SHARED;
    else if (!profile_path.empty())
      profile_type = PROFILE_USER;
  } else {
    VLOG(1) << "No profile path for service " << service_path << ".";
  }

  scoped_ptr<NetworkUIData> ui_data = GetUIData(shill_properties);

  const base::DictionaryValue* user_settings = NULL;
  const base::DictionaryValue* shared_settings = NULL;

  if (ui_data) {
    if (profile_type == PROFILE_SHARED)
      shared_settings = ui_data->user_settings();
    else if (profile_type == PROFILE_USER)
      user_settings = ui_data->user_settings();
  } else if (profile_type != PROFILE_NONE) {
    LOG(WARNING) << "Service " << service_path << " of profile "
                 << profile_path << " contains no or no valid UIData.";
    // TODO(pneubeck): add a conversion of user configured entries of old
    // ChromeOS versions. We will have to use a heuristic to determine which
    // properties _might_ be user configured.
  }

  scoped_ptr<base::DictionaryValue> active_settings(
      onc::TranslateShillServiceToONCPart(
          shill_properties,
          &onc::kNetworkWithStateSignature));

  std::string guid;
  active_settings->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                 &guid);

  const base::DictionaryValue* user_policy = NULL;
  const base::DictionaryValue* device_policy = NULL;
  if (!guid.empty()) {
    // We already checked that the policies were initialized. No need to do that
    // again.
    if (profile_type == PROFILE_SHARED)
      device_policy = device_policies_by_guid_[guid];
    else if (profile_type == PROFILE_USER)
      user_policy = user_policies_by_guid_[guid];
  }

  // This call also removes credentials from policies.
  scoped_ptr<base::DictionaryValue> augmented_properties =
      onc::MergeSettingsAndPoliciesToAugmented(
          onc::kNetworkConfigurationSignature,
          user_policy,
          device_policy,
          user_settings,
          shared_settings,
          active_settings.get());
  callback.Run(service_path, *augmented_properties);
}

void ManagedNetworkConfigurationHandler::GetProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->GetProperties(
      service_path,
      base::Bind(&TranslatePropertiesToOncAndRunCallback, callback),
      error_callback);
}

void ManagedNetworkConfigurationHandler::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& user_settings,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  const NetworkState* state =
      NetworkStateHandler::Get()->GetNetworkState(service_path);

  if (!state) {
    RunErrorCallback(service_path,
                     kUnknownServicePath,
                     kUnknownServicePathMessage,
                     error_callback);
    return;
  }

  std::string guid = state->guid();
  if (guid.empty()) {
    // TODO(pneubeck): create an initial configuration in this case. As for
    // CreateConfiguration, user settings from older ChromeOS versions have to
    // determined here.
    RunErrorCallback(service_path,
                     kSetOnUnconfiguredNetwork,
                     kSetOnUnconfiguredNetworkMessage,
                     error_callback);
    return;
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  onc::Validator validator(false,  // Ignore unknown fields.
                           false,  // Ignore invalid recommended field names.
                           false,  // Ignore missing fields.
                           false);  // This ONC does not comes from policy.

  onc::Validator::Result validation_result;
  scoped_ptr<base::DictionaryValue> validated_user_settings =
      validator.ValidateAndRepairObject(
          &onc::kNetworkConfigurationSignature,
          user_settings,
          &validation_result);

  if (validation_result == onc::Validator::INVALID) {
    LOG(ERROR) << "ONC user settings are invalid and couldn't be repaired.";
    RunErrorCallback(service_path,
                     kInvalidUserSettings,
                     kInvalidUserSettingsMessage,
                     error_callback);
    return;
  }
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    LOG(WARNING) << "Validation of ONC user settings produced warnings.";

  VLOG(2) << "SetProperties: Found GUID " << guid << " and profile "
          << state->profile_path();

  const PolicyMap* policies_by_guid =
      GetPoliciesForProfile(state->profile_path());

  if (!policies_by_guid) {
    RunErrorCallback(service_path,
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }

  const base::DictionaryValue* policy = NULL;
  PolicyMap::const_iterator it = policies_by_guid->find(guid);
  if (it != policies_by_guid->end())
    policy = it->second;

  VLOG(2) << "This configuration is " << (policy ? "" : "not ") << "managed.";

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      CreateShillConfiguration(state->profile_path(), guid, policy,
                               &user_settings));

  NetworkConfigurationHandler::Get()->SetProperties(service_path,
                                                    *shill_dictionary,
                                                    callback,
                                                    error_callback);
}

void ManagedNetworkConfigurationHandler::Connect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->Connect(service_path,
                                              callback,
                                              error_callback);
}

void ManagedNetworkConfigurationHandler::Disconnect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->Disconnect(service_path,
                                                 callback,
                                                 error_callback);
}

void ManagedNetworkConfigurationHandler::CreateConfiguration(
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  std::string profile_path = kUserProfilePath;
  const PolicyMap* policies_by_guid = GetPoliciesForProfile(profile_path);

  if (!policies_by_guid) {
    RunErrorCallback("",
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }

  if (FindMatchingPolicy(*policies_by_guid, properties)) {
    RunErrorCallback("",
                     kNetworkAlreadyConfigured,
                     kNetworkAlreadyConfiguredMessage,
                     error_callback);
  }

  // TODO(pneubeck): In case of WiFi, check that no other configuration for the
  // same {SSID, mode, security} exists. We don't support such multiple
  // configurations, yet.

  // Generate a new GUID for this configuration. Ignore the maybe provided GUID
  // in |properties| as it is not our own and from an untrusted source.
  std::string guid = base::GenerateGUID();

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      CreateShillConfiguration(profile_path, guid, NULL /*no policy*/,
                               &properties));

  NetworkConfigurationHandler::Get()->CreateConfiguration(*shill_dictionary,
                                                          callback,
                                                          error_callback);
}

void ManagedNetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->RemoveConfiguration(service_path,
                                                          callback,
                                                          error_callback);
}

// This class compares (entry point is Run()) |modified_policies| with the
// existing entries in the provided Shill profile |profile|. It fetches all
// entries in parallel (GetProfileProperties), compares each entry with the
// current policies (GetEntry) and adds all missing policies
// (~PolicyApplicator).
class ManagedNetworkConfigurationHandler::PolicyApplicator
    : public base::RefCounted<PolicyApplicator> {
 public:
  typedef ManagedNetworkConfigurationHandler::PolicyMap PolicyMap;

  // |modified_policies| must not be NULL and will be empty afterwards.
  PolicyApplicator(base::WeakPtr<ManagedNetworkConfigurationHandler> handler,
                   const std::string& profile,
                   std::set<std::string>* modified_policies)
      : handler_(handler),
        profile_path_(profile) {
    remaining_policies_.swap(*modified_policies);
  }

  void Run() {
    DBusThreadManager::Get()->GetShillProfileClient()->GetProperties(
        dbus::ObjectPath(profile_path_),
        base::Bind(&PolicyApplicator::GetProfileProperties, this),
        base::Bind(&LogErrorMessage, FROM_HERE));
  }

 private:
  friend class base::RefCounted<PolicyApplicator>;

  void GetProfileProperties(const base::DictionaryValue& profile_properties) {
    if (!handler_) {
      LOG(WARNING) << "Handler destructed during policy application to profile "
                   << profile_path_;
      return;
    }

    VLOG(2) << "Received properties for profile " << profile_path_;
    const base::ListValue* entries = NULL;
    if (!profile_properties.GetListWithoutPathExpansion(
            flimflam::kEntriesProperty, &entries)) {
      LOG(ERROR) << "Profile " << profile_path_
                 << " doesn't contain the property "
                 << flimflam::kEntriesProperty;
      return;
    }

    for (base::ListValue::const_iterator it = entries->begin();
         it != entries->end(); ++it) {
      std::string entry;
      (*it)->GetAsString(&entry);

      std::ostringstream entry_failure;
      DBusThreadManager::Get()->GetShillProfileClient()->GetEntry(
        dbus::ObjectPath(profile_path_),
        entry,
        base::Bind(&PolicyApplicator::GetEntry, this, entry),
        base::Bind(&LogErrorMessage, FROM_HERE));
    }
  }

  void GetEntry(const std::string& entry,
                const base::DictionaryValue& entry_properties) {
    if (!handler_) {
      LOG(WARNING) << "Handler destructed during policy application to profile "
                   << profile_path_;
      return;
    }

    VLOG(2) << "Received properties for entry " << entry << " of profile "
            << profile_path_;

    scoped_ptr<base::DictionaryValue> onc_part(
        onc::TranslateShillServiceToONCPart(
            entry_properties,
            &onc::kNetworkWithStateSignature));

    std::string old_guid;
    if (!onc_part->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                 &old_guid)) {
      LOG(WARNING) << "Entry " << entry << " of profile " << profile_path_
                   << " doesn't contain a GUID.";
      // This might be an entry of an older ChromeOS version. Assume it to be
      // unmanaged.
      return;
    }

    scoped_ptr<NetworkUIData> ui_data = GetUIData(entry_properties);
    if (!ui_data) {
      VLOG(1) << "Entry " << entry << " of profile " << profile_path_
              << " contains no or no valid UIData.";
      // This might be an entry of an older ChromeOS version. Assume it to be
      // unmanaged.
      return;
    }

    bool was_managed =
        (ui_data->onc_source() == onc::ONC_SOURCE_DEVICE_POLICY ||
         ui_data->onc_source() == onc::ONC_SOURCE_USER_POLICY);

    // The relevant policy must have been initialized, otherwise we hadn't Run
    // this PolicyApplicator.
    const PolicyMap& policies_by_guid =
        *handler_->GetPoliciesForProfile(profile_path_);

    const base::DictionaryValue* new_policy = NULL;
    if (was_managed) {
      // If we have a GUID that might match a current policy, do a lookup using
      // that GUID at first. In particular this is necessary, as some networks
      // can't be matched to policies by properties (e.g. VPN).
      PolicyMap::const_iterator it = policies_by_guid.find(old_guid);
      if (it != policies_by_guid.end())
        new_policy = it->second;
    }

    if (!new_policy) {
      // If we didn't find a policy by GUID, still a new policy might match.
      new_policy = FindMatchingPolicy(policies_by_guid, *onc_part);
    }

    if (new_policy) {
      std::string new_guid;
      new_policy->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                &new_guid);

      VLOG_IF(1, was_managed && old_guid != new_guid)
          << "Updating configuration previously managed by policy " << old_guid
          << " with new policy " << new_guid << ".";
      VLOG_IF(1, !was_managed)
          << "Applying policy " << new_guid << " to previously unmanaged "
          << "configuration.";

      if (old_guid == new_guid &&
          remaining_policies_.find(new_guid) == remaining_policies_.end()) {
        VLOG(1) << "Not updating existing managed configuration with guid "
                << new_guid << " because the policy didn't change.";
      } else {
        VLOG_IF(1, old_guid == new_guid)
            << "Updating previously managed configuration with the updated "
            << "policy " << new_guid << ".";

        // Update the existing configuration with the maybe changed
        // policy. Thereby the GUID might change.
        scoped_ptr<base::DictionaryValue> shill_dictionary =
            CreateShillConfiguration(profile_path_, new_guid, new_policy,
                                     ui_data->user_settings());
        NetworkConfigurationHandler::Get()->CreateConfiguration(
            *shill_dictionary,
            base::Bind(&IgnoreString),
            base::Bind(&LogErrorWithDict, FROM_HERE));
        remaining_policies_.erase(new_guid);
      }
    } else if (was_managed) {
      VLOG(1) << "Removing configuration previously managed by policy "
              << old_guid << ", because the policy was removed.";

      // Remove the entry, because the network was managed but isn't anymore.
      // Note: An alternative might be to preserve the user settings, but it's
      // unclear which values originating the policy should be removed.
      DeleteEntry(entry);
    } else {
      VLOG(2) << "Ignore unmanaged entry.";

      // The entry wasn't managed and doesn't match any current policy. Thus
      // leave it as it is.
    }
  }

  void DeleteEntry(const std::string& entry) {
    DBusThreadManager::Get()->GetShillProfileClient()->DeleteEntry(
        dbus::ObjectPath(profile_path_),
        entry,
        base::Bind(&base::DoNothing),
        base::Bind(&LogErrorMessage, FROM_HERE));
  }

  virtual ~PolicyApplicator() {
    if (remaining_policies_.empty())
      return;

    VLOG(2) << "Create new managed network configurations in profile"
            << profile_path_ << ".";
    // All profile entries were compared to policies. |configureGUIDs_| contains
    // all matched policies. From the remainder of policies, new configurations
    // have to be created.

    // The relevant policy must have been initialized, otherwise we hadn't Run
    // this PolicyApplicator.
    const PolicyMap& policies_by_guid =
        *handler_->GetPoliciesForProfile(profile_path_);

    for (std::set<std::string>::iterator it = remaining_policies_.begin();
         it != remaining_policies_.end(); ++it) {
      PolicyMap::const_iterator policy_it = policies_by_guid.find(*it);
      if (policy_it == policies_by_guid.end()) {
        LOG(ERROR) << "Policy " << *it << " doesn't exist anymore.";
        continue;
      }

      const base::DictionaryValue* policy = policy_it->second;

      VLOG(1) << "Creating new configuration managed by policy " << *it
              << " in profile " << profile_path_ << ".";

      scoped_ptr<base::DictionaryValue> shill_dictionary =
          CreateShillConfiguration(profile_path_, *it, policy, NULL);
      NetworkConfigurationHandler::Get()->CreateConfiguration(
          *shill_dictionary,
          base::Bind(&IgnoreString),
          base::Bind(&LogErrorWithDict, FROM_HERE));
    }
  }

  std::set<std::string> remaining_policies_;
  base::WeakPtr<ManagedNetworkConfigurationHandler> handler_;
  std::string profile_path_;

  DISALLOW_COPY_AND_ASSIGN(PolicyApplicator);
};

void ManagedNetworkConfigurationHandler::SetPolicy(
    onc::ONCSource onc_source,
    const base::ListValue& network_configs_onc) {
  VLOG(1) << "Setting policies for ONC source "
          << onc::GetSourceAsString(onc_source) << ".";

  PolicyMap* policies;
  std::string profile;
  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY) {
    policies = &user_policies_by_guid_;
    profile = kUserProfilePath;
    user_policies_initialized_ = true;
  } else {
    policies = &device_policies_by_guid_;
    profile = kSharedProfilePath;
    device_policies_initialized_ = true;
  }

  PolicyMap old_policies;
  policies->swap(old_policies);

  // This stores all GUIDs of policies that have changed or are new.
  std::set<std::string> modified_policies;

  for (base::ListValue::const_iterator it = network_configs_onc.begin();
       it != network_configs_onc.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    std::string guid;
    network->GetStringWithoutPathExpansion(onc::network_config::kGUID, &guid);
    DCHECK(!guid.empty());

    if (policies->count(guid) > 0) {
      LOG(ERROR) << "ONC from " << onc::GetSourceAsString(onc_source)
                 << " contains several entries for the same GUID "
                 << guid << ".";
      delete (*policies)[guid];
    }
    const base::DictionaryValue* new_entry = network->DeepCopy();
    (*policies)[guid] = new_entry;

    const base::DictionaryValue* old_entry = old_policies[guid];
    if (!old_entry || !old_entry->Equals(new_entry))
      modified_policies.insert(guid);
  }

  STLDeleteValues(&old_policies);

  scoped_refptr<PolicyApplicator> applicator = new PolicyApplicator(
      weak_ptr_factory_.GetWeakPtr(),
      profile,
      &modified_policies);
  applicator->Run();
}

const ManagedNetworkConfigurationHandler::PolicyMap*
ManagedNetworkConfigurationHandler::GetPoliciesForProfile(
    const std::string& profile) const {
  if (profile == kSharedProfilePath) {
    if (device_policies_initialized_)
      return &device_policies_by_guid_;
  } else if (user_policies_initialized_) {
    return &user_policies_by_guid_;
  }
  return NULL;
}

ManagedNetworkConfigurationHandler::ManagedNetworkConfigurationHandler()
    : user_policies_initialized_(false),
      device_policies_initialized_(false),
      weak_ptr_factory_(this) {
}

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() {
  STLDeleteValues(&user_policies_by_guid_);
  STLDeleteValues(&device_policies_by_guid_);
}

}  // namespace chromeos

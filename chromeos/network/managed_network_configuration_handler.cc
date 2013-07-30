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
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kInvalidUserSettingsMessage[] = "User settings are invalid.";
const char kInvalidUserSettings[] = "Error.InvalidUserSettings";
const char kNetworkAlreadyConfiguredMessage[] =
    "Network is already configured.";
const char kNetworkAlreadyConfigured[] = "Error.NetworkAlreadyConfigured";
const char kPoliciesNotInitializedMessage[] = "Policies not initialized.";
const char kPoliciesNotInitialized[] = "Error.PoliciesNotInitialized";
const char kProfileNotInitializedMessage[] = "Profile not initialized.";
const char kProfileNotInitialized[] = "Error.ProflieNotInitialized";
const char kSetOnUnconfiguredNetworkMessage[] =
    "Unable to modify properties of an unconfigured network.";
const char kSetOnUnconfiguredNetwork[] = "Error.SetCalledOnUnconfiguredNetwork";
const char kUnknownProfilePathMessage[] = "Profile path is unknown.";
const char kUnknownProfilePath[] = "Error.UnknownProfilePath";
const char kUnknownServicePathMessage[] = "Service path is unknown.";
const char kUnknownServicePath[] = "Error.UnknownServicePath";

// This fake credential contains a random postfix which is extremly unlikely to
// be used by any user.
const char kFakeCredential[] = "FAKE_CREDENTIAL_VPaJDV9x";

std::string ToDebugString(onc::ONCSource source,
                          const std::string& userhash) {
  return source == onc::ONC_SOURCE_USER_POLICY ?
      ("user policy of " + userhash) : "device policy";
}

void RunErrorCallback(const std::string& service_path,
                      const std::string& error_name,
                      const std::string& error_message,
                      const network_handler::ErrorCallback& error_callback) {
  NET_LOG_ERROR(error_name, error_message);
  error_callback.Run(
      error_name,
      make_scoped_ptr(
          network_handler::CreateErrorData(service_path,
                                           error_name,
                                           error_message)));
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
                      scoped_ptr<base::DictionaryValue> error_data) {
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
// type, |policy| is interpreted as the user or device policy and |settings| as
// the user or shared settings.
scoped_ptr<base::DictionaryValue> CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::DictionaryValue* policy,
    const base::DictionaryValue* settings) {
  scoped_ptr<base::DictionaryValue> effective;
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  if (policy) {
    if (profile.type() == NetworkProfile::TYPE_SHARED) {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          NULL,  // no user policy
          policy,  // device policy
          NULL,  // no user settings
          settings);  // shared settings
      onc_source = onc::ONC_SOURCE_DEVICE_POLICY;
    } else if (profile.type() == NetworkProfile::TYPE_USER) {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          policy,  // user policy
          NULL,  // no device policy
          settings,  // user settings
          NULL);  // no shared settings
      onc_source = onc::ONC_SOURCE_USER_POLICY;
    } else {
      NOTREACHED();
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

  // Remove irrelevant fields.
  onc::Normalizer normalizer(true /* remove recommended fields */);
  effective = normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                         *effective);

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                     *effective));

  shill_dictionary->SetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                                  profile.path);

  scoped_ptr<NetworkUIData> ui_data;
  if (policy)
    ui_data = NetworkUIData::CreateFromONC(onc_source, *policy);
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

// Returns true if |policy| matches |actual_network|, which must be part of a
// ONC NetworkConfiguration. This should be the only such matching function
// within Chrome. Shill does such matching in several functions for network
// identification. For compatibility, we currently should stick to Shill's
// matching behavior.
bool IsPolicyMatching(const base::DictionaryValue& policy,
                      const base::DictionaryValue& actual_network) {
  std::string policy_type;
  policy.GetStringWithoutPathExpansion(onc::network_config::kType,
                                       &policy_type);
  std::string network_type;
  actual_network.GetStringWithoutPathExpansion(onc::network_config::kType,
                                               &network_type);
  if (policy_type != network_type)
    return false;

  if (network_type != onc::network_type::kWiFi)
    return false;

  const base::DictionaryValue* policy_wifi = NULL;
  policy.GetDictionaryWithoutPathExpansion(onc::network_config::kWiFi,
                                           &policy_wifi);
  const base::DictionaryValue* actual_wifi = NULL;
  actual_network.GetDictionaryWithoutPathExpansion(onc::network_config::kWiFi,
                                                   &actual_wifi);
  if (!policy_wifi || !actual_wifi)
    return false;

  std::string policy_ssid;
  policy_wifi->GetStringWithoutPathExpansion(onc::wifi::kSSID, &policy_ssid);
  std::string actual_ssid;
  actual_wifi->GetStringWithoutPathExpansion(onc::wifi::kSSID, &actual_ssid);
  return (policy_ssid == actual_ssid);
}

// Returns the policy from |policies| matching |actual_network|, if any exists.
// Returns NULL otherwise. |actual_network| must be part of a ONC
// NetworkConfiguration.
const base::DictionaryValue* FindMatchingPolicy(
    const ManagedNetworkConfigurationHandler::GuidToPolicyMap &policies,
    const base::DictionaryValue& actual_network) {
  for (ManagedNetworkConfigurationHandler::GuidToPolicyMap::const_iterator it =
           policies.begin(); it != policies.end(); ++it) {
    if (IsPolicyMatching(*it->second, actual_network))
      return it->second;
  }
  return NULL;
}

const base::DictionaryValue* GetByGUID(
    const ManagedNetworkConfigurationHandler::GuidToPolicyMap &policies,
    const std::string& guid) {
  ManagedNetworkConfigurationHandler::GuidToPolicyMap::const_iterator it =
      policies.find(guid);
  if (it == policies.end())
    return NULL;
  return it->second;
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

// This class compares (entry point is Run()) |modified_policies| with the
// existing entries in the provided Shill profile |profile|. It fetches all
// entries in parallel (GetProfilePropertiesCallback), compares each entry with
// the current policies (GetEntryCallback) and adds all missing policies
// (~PolicyApplicator).
class ManagedNetworkConfigurationHandler::PolicyApplicator
    : public base::RefCounted<PolicyApplicator> {
 public:
  typedef ManagedNetworkConfigurationHandler::GuidToPolicyMap GuidToPolicyMap;

  // |modified_policies| must not be NULL and will be empty afterwards.
  PolicyApplicator(base::WeakPtr<ManagedNetworkConfigurationHandler> handler,
                   const NetworkProfile& profile,
                   std::set<std::string>* modified_policies);

  void Run();

 private:
  friend class base::RefCounted<PolicyApplicator>;

  // Called with the properties of the profile |profile_|. Requests the
  // properties of each entry, which are processed by GetEntryCallback.
  void GetProfilePropertiesCallback(
      const base::DictionaryValue& profile_properties);

  // Called with the properties of the profile entry |entry|. Checks whether the
  // entry was previously managed, whether a current policy applies and then
  // either updates, deletes or not touches the entry.
  void GetEntryCallback(const std::string& entry,
                        const base::DictionaryValue& entry_properties);

  // Sends Shill the command to delete profile entry |entry| from |profile_|.
  void DeleteEntry(const std::string& entry);

  // Creates new entries for all remaining policies, i.e. for which not matching
  // entry was found.
  virtual ~PolicyApplicator();

  std::set<std::string> remaining_policies_;
  base::WeakPtr<ManagedNetworkConfigurationHandler> handler_;
  NetworkProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(PolicyApplicator);
};

// static
scoped_ptr<NetworkUIData> ManagedNetworkConfigurationHandler::GetUIData(
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
  VLOG(2) << "JSON dictionary has no UIData blob: " << shill_dictionary;
  return scoped_ptr<NetworkUIData>();
}

void ManagedNetworkConfigurationHandler::GetManagedProperties(
    const std::string& userhash,
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (!GetPoliciesForUser(userhash) || !GetPoliciesForUser(std::string())) {
    RunErrorCallback(service_path,
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }
  network_configuration_handler_->GetProperties(
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
  shill_properties.GetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                                 &profile_path);
  LOG(ERROR) << "Profile: " << profile_path;
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    LOG(ERROR) << "No or no known profile received for service "
            << service_path << ".";
  }

  scoped_ptr<NetworkUIData> ui_data = GetUIData(shill_properties);

  const base::DictionaryValue* user_settings = NULL;
  const base::DictionaryValue* shared_settings = NULL;

  if (ui_data && profile) {
    if (profile->type() == NetworkProfile::TYPE_SHARED)
      shared_settings = ui_data->user_settings();
    else if (profile->type() == NetworkProfile::TYPE_USER)
      user_settings = ui_data->user_settings();
    else
      NOTREACHED();
  } else if (profile) {
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
  if (!guid.empty() && profile) {
    const GuidToPolicyMap* policies = GetPoliciesForProfile(*profile);
    if (!policies) {
      RunErrorCallback(service_path,
                       kPoliciesNotInitialized,
                       kPoliciesNotInitializedMessage,
                       error_callback);
      return;
    }
    const base::DictionaryValue* policy = GetByGUID(*policies, guid);
    if (profile->type() == NetworkProfile::TYPE_SHARED)
      device_policy = policy;
    else if (profile->type() == NetworkProfile::TYPE_USER)
      user_policy = policy;
    else
      NOTREACHED();
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
  network_configuration_handler_->GetProperties(
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
      network_state_handler_->GetNetworkState(service_path);

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

  const std::string& profile_path = state->profile_path();
  const NetworkProfile *profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    RunErrorCallback(service_path,
                     kUnknownProfilePath,
                     kUnknownProfilePathMessage,
                     error_callback);
    return;
  }

  VLOG(2) << "SetProperties: Found GUID " << guid << " and profile "
          << profile->ToDebugString();

  const GuidToPolicyMap* policies = GetPoliciesForProfile(*profile);
  if (!policies) {
    RunErrorCallback(service_path,
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  onc::Validator validator(false,  // Ignore unknown fields.
                           false,  // Ignore invalid recommended field names.
                           false,  // Ignore missing fields.
                           false);  // This ONC does not come from policy.

  onc::Validator::Result validation_result;
  scoped_ptr<base::DictionaryValue> validated_user_settings =
      validator.ValidateAndRepairObject(
          &onc::kNetworkConfigurationSignature,
          user_settings,
          &validation_result);

  if (validation_result == onc::Validator::INVALID) {
    RunErrorCallback(service_path,
                     kInvalidUserSettings,
                     kInvalidUserSettingsMessage,
                     error_callback);
    return;
  }
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    LOG(WARNING) << "Validation of ONC user settings produced warnings.";

  const base::DictionaryValue* policy = GetByGUID(*policies, guid);
  VLOG(2) << "This configuration is " << (policy ? "" : "not ") << "managed.";

  scoped_ptr<base::DictionaryValue> shill_dictionary(CreateShillConfiguration(
      *profile, guid, policy, validated_user_settings.get()));

  network_configuration_handler_->SetProperties(
      service_path, *shill_dictionary, callback, error_callback);
}

void ManagedNetworkConfigurationHandler::CreateConfiguration(
    const std::string& userhash,
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  const GuidToPolicyMap* policies = GetPoliciesForUser(userhash);
  if (!policies) {
    RunErrorCallback("",
                     kPoliciesNotInitialized,
                     kPoliciesNotInitializedMessage,
                     error_callback);
    return;
  }

  if (FindMatchingPolicy(*policies, properties)) {
    RunErrorCallback("",
                     kNetworkAlreadyConfigured,
                     kNetworkAlreadyConfiguredMessage,
                     error_callback);
  }

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    RunErrorCallback("",
                     kProfileNotInitialized,
                     kProfileNotInitializedMessage,
                     error_callback);
  }

  // TODO(pneubeck): In case of WiFi, check that no other configuration for the
  // same {SSID, mode, security} exists. We don't support such multiple
  // configurations, yet.

  // Generate a new GUID for this configuration. Ignore the maybe provided GUID
  // in |properties| as it is not our own and from an untrusted source.
  std::string guid = base::GenerateGUID();
  scoped_ptr<base::DictionaryValue> shill_dictionary(
      CreateShillConfiguration(*profile, guid, NULL /*no policy*/,
                               &properties));

  network_configuration_handler_->CreateConfiguration(
      *shill_dictionary, callback, error_callback);
}

void ManagedNetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  network_configuration_handler_->RemoveConfiguration(
      service_path, callback, error_callback);
}

void ManagedNetworkConfigurationHandler::SetPolicy(
    onc::ONCSource onc_source,
    const std::string& userhash,
    const base::ListValue& network_configs_onc) {
  VLOG(1) << "Setting policies from " << ToDebugString(onc_source, userhash)
          << ".";

  // |userhash| must be empty for device policies.
  DCHECK(onc_source != chromeos::onc::ONC_SOURCE_DEVICE_POLICY ||
         userhash.empty());
  GuidToPolicyMap& policies = policies_by_user_[userhash];

  GuidToPolicyMap old_policies;
  policies.swap(old_policies);

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

    if (policies.count(guid) > 0) {
      LOG(ERROR) << "ONC from " << ToDebugString(onc_source, userhash)
                 << " contains several entries for the same GUID "
                 << guid << ".";
      delete policies[guid];
    }
    const base::DictionaryValue* new_entry = network->DeepCopy();
    policies[guid] = new_entry;

    const base::DictionaryValue* old_entry = old_policies[guid];
    if (!old_entry || !old_entry->Equals(new_entry))
      modified_policies.insert(guid);
  }

  STLDeleteValues(&old_policies);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    VLOG(1) << "The relevant Shill profile isn't initialized yet, postponing "
            << "policy application.";
    return;
  }

  scoped_refptr<PolicyApplicator> applicator = new PolicyApplicator(
      weak_ptr_factory_.GetWeakPtr(),
      *profile,
      &modified_policies);
  applicator->Run();
}

void ManagedNetworkConfigurationHandler::OnProfileAdded(
    const NetworkProfile& profile) {
  VLOG(1) << "Adding profile " << profile.ToDebugString() << "'.";

  const GuidToPolicyMap* policies = GetPoliciesForProfile(profile);
  if (!policies) {
    VLOG(1) << "The relevant policy is not initialized, "
            << "postponing policy application.";
    return;
  }

  std::set<std::string> policy_guids;
  for (GuidToPolicyMap::const_iterator it = policies->begin();
       it != policies->end(); ++it) {
    policy_guids.insert(it->first);
  }

  scoped_refptr<PolicyApplicator> applicator = new PolicyApplicator(
      weak_ptr_factory_.GetWeakPtr(),
      profile,
      &policy_guids);
  applicator->Run();
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandler::FindPolicyByGUID(
    const std::string userhash,
    const std::string& guid,
    onc::ONCSource* onc_source) const {
  *onc_source = onc::ONC_SOURCE_NONE;

  if (!userhash.empty()) {
    const GuidToPolicyMap* user_policies = GetPoliciesForUser(userhash);
    if (user_policies) {
      GuidToPolicyMap::const_iterator found = user_policies->find(guid);
      if (found != user_policies->end()) {
        *onc_source = onc::ONC_SOURCE_USER_POLICY;
        return found->second;
      }
    }
  }

  const GuidToPolicyMap* device_policies = GetPoliciesForUser(std::string());
  if (device_policies) {
    GuidToPolicyMap::const_iterator found = device_policies->find(guid);
    if (found != device_policies->end()) {
      *onc_source = onc::ONC_SOURCE_DEVICE_POLICY;
      return found->second;
    }
  }

  return NULL;
}

void ManagedNetworkConfigurationHandler::OnProfileRemoved(
    const NetworkProfile& profile) {
  // Nothing to do in this case.
}

const ManagedNetworkConfigurationHandler::GuidToPolicyMap*
ManagedNetworkConfigurationHandler::GetPoliciesForUser(
    const std::string& userhash) const {
  UserToPoliciesMap::const_iterator it = policies_by_user_.find(userhash);
  if (it == policies_by_user_.end())
    return NULL;
  return &it->second;
}

const ManagedNetworkConfigurationHandler::GuidToPolicyMap*
ManagedNetworkConfigurationHandler::GetPoliciesForProfile(
    const NetworkProfile& profile) const {
  DCHECK(profile.type() != NetworkProfile::TYPE_SHARED ||
         profile.userhash.empty());
  return GetPoliciesForUser(profile.userhash);
}

ManagedNetworkConfigurationHandler::ManagedNetworkConfigurationHandler()
    : network_state_handler_(NULL),
      network_profile_handler_(NULL),
      network_configuration_handler_(NULL),
      weak_ptr_factory_(this) {
}

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() {
  network_profile_handler_->RemoveObserver(this);
  for (UserToPoliciesMap::iterator it = policies_by_user_.begin();
       it != policies_by_user_.end(); ++it) {
    STLDeleteValues(&it->second);
  }
}

void ManagedNetworkConfigurationHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler) {
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_profile_handler_->AddObserver(this);
}

ManagedNetworkConfigurationHandler::PolicyApplicator::PolicyApplicator(
    base::WeakPtr<ManagedNetworkConfigurationHandler> handler,
    const NetworkProfile& profile,
    std::set<std::string>* modified_policies)
    : handler_(handler), profile_(profile) {
  remaining_policies_.swap(*modified_policies);
}

void ManagedNetworkConfigurationHandler::PolicyApplicator::Run() {
  DBusThreadManager::Get()->GetShillProfileClient()->GetProperties(
      dbus::ObjectPath(profile_.path),
      base::Bind(&PolicyApplicator::GetProfilePropertiesCallback, this),
      base::Bind(&LogErrorMessage, FROM_HERE));
}

void ManagedNetworkConfigurationHandler::PolicyApplicator::
    GetProfilePropertiesCallback(
        const base::DictionaryValue& profile_properties) {
  if (!handler_) {
    LOG(WARNING) << "Handler destructed during policy application to profile "
                 << profile_.ToDebugString();
    return;
  }

  VLOG(2) << "Received properties for profile " << profile_.ToDebugString();
  const base::ListValue* entries = NULL;
  if (!profile_properties.GetListWithoutPathExpansion(
          flimflam::kEntriesProperty, &entries)) {
    LOG(ERROR) << "Profile " << profile_.ToDebugString()
               << " doesn't contain the property "
               << flimflam::kEntriesProperty;
    return;
  }

  for (base::ListValue::const_iterator it = entries->begin();
       it != entries->end();
       ++it) {
    std::string entry;
    (*it)->GetAsString(&entry);

    std::ostringstream entry_failure;
    DBusThreadManager::Get()->GetShillProfileClient()
        ->GetEntry(dbus::ObjectPath(profile_.path),
                   entry,
                   base::Bind(&PolicyApplicator::GetEntryCallback, this, entry),
                   base::Bind(&LogErrorMessage, FROM_HERE));
  }
}

void ManagedNetworkConfigurationHandler::PolicyApplicator::GetEntryCallback(
    const std::string& entry,
    const base::DictionaryValue& entry_properties) {
  if (!handler_) {
    LOG(WARNING) << "Handler destructed during policy application to profile "
                 << profile_.ToDebugString();
    return;
  }

  VLOG(2) << "Received properties for entry " << entry << " of profile "
          << profile_.ToDebugString();

  scoped_ptr<base::DictionaryValue> onc_part(
      onc::TranslateShillServiceToONCPart(entry_properties,
                                          &onc::kNetworkWithStateSignature));

  std::string old_guid;
  if (!onc_part->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                               &old_guid)) {
    VLOG(1) << "Entry " << entry << " of profile " << profile_.ToDebugString()
            << " doesn't contain a GUID.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged.
  }

  scoped_ptr<NetworkUIData> ui_data = GetUIData(entry_properties);
  if (!ui_data) {
    VLOG(1) << "Entry " << entry << " of profile " << profile_.ToDebugString()
            << " contains no or no valid UIData.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged. It's an inconsistency if there is a GUID but no UIData, thus
    // clear the GUID just in case.
    old_guid.clear();
  }

  bool was_managed = !old_guid.empty() && ui_data &&
                     (ui_data->onc_source() == onc::ONC_SOURCE_DEVICE_POLICY ||
                      ui_data->onc_source() == onc::ONC_SOURCE_USER_POLICY);

  // The relevant policy must have been initialized, otherwise we hadn't Run
  // this PolicyApplicator.
  const GuidToPolicyMap& policies = *handler_->GetPoliciesForProfile(profile_);

  const base::DictionaryValue* new_policy = NULL;
  if (was_managed) {
    // If we have a GUID that might match a current policy, do a lookup using
    // that GUID at first. In particular this is necessary, as some networks
    // can't be matched to policies by properties (e.g. VPN).
    new_policy = GetByGUID(policies, old_guid);
  }

  if (!new_policy) {
    // If we didn't find a policy by GUID, still a new policy might match.
    new_policy = FindMatchingPolicy(policies, *onc_part);
  }

  if (new_policy) {
    std::string new_guid;
    new_policy->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                              &new_guid);

    VLOG_IF(1, was_managed && old_guid != new_guid)
        << "Updating configuration previously managed by policy " << old_guid
        << " with new policy " << new_guid << ".";
    VLOG_IF(1, !was_managed) << "Applying policy " << new_guid
                             << " to previously unmanaged "
                             << "configuration.";

    if (old_guid == new_guid &&
        remaining_policies_.find(new_guid) == remaining_policies_.end()) {
      VLOG(1) << "Not updating existing managed configuration with guid "
              << new_guid << " because the policy didn't change.";
    } else {
      // Delete the entry to ensure that no old configuration remains.
      // Don't do this if a policy is reapplied (e.g. after reboot) or updated
      // (i.e. the GUID didn't change), in order to keep implicit state of
      // Shill like "connected successfully before".
      if (old_guid == new_guid) {
        VLOG(1) << "Updating previously managed configuration with the "
                << "updated policy " << new_guid << ".";
      } else {
        DeleteEntry(entry);
      }

      const base::DictionaryValue* user_settings =
          ui_data ? ui_data->user_settings() : NULL;

      // Write the new configuration.
      scoped_ptr<base::DictionaryValue> shill_dictionary =
          CreateShillConfiguration(
              profile_, new_guid, new_policy, user_settings);
      handler_->network_configuration_handler()
          ->CreateConfiguration(*shill_dictionary,
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

void ManagedNetworkConfigurationHandler::PolicyApplicator::DeleteEntry(
    const std::string& entry) {
  DBusThreadManager::Get()->GetShillProfileClient()
      ->DeleteEntry(dbus::ObjectPath(profile_.path),
                    entry,
                    base::Bind(&base::DoNothing),
                    base::Bind(&LogErrorMessage, FROM_HERE));
}

ManagedNetworkConfigurationHandler::PolicyApplicator::~PolicyApplicator() {
  if (!handler_) {
    LOG(WARNING) << "Handler destructed during policy application to profile "
                 << profile_.ToDebugString();
    return;
  }

  if (remaining_policies_.empty())
    return;

  VLOG(2) << "Create new managed network configurations in profile"
          << profile_.ToDebugString() << ".";
  // All profile entries were compared to policies. |configureGUIDs_| contains
  // all matched policies. From the remainder of policies, new configurations
  // have to be created.

  // The relevant policy must have been initialized, otherwise we hadn't Run
  // this PolicyApplicator.
  const GuidToPolicyMap& policies = *handler_->GetPoliciesForProfile(profile_);

  for (std::set<std::string>::iterator it = remaining_policies_.begin();
       it != remaining_policies_.end();
       ++it) {
    const base::DictionaryValue* policy = GetByGUID(policies, *it);
    if (!policy) {
      LOG(ERROR) << "Policy " << *it << " doesn't exist anymore.";
      continue;
    }

    VLOG(1) << "Creating new configuration managed by policy " << *it
            << " in profile " << profile_.ToDebugString() << ".";

    scoped_ptr<base::DictionaryValue> shill_dictionary =
        CreateShillConfiguration(
            profile_, *it, policy, NULL /* no user settings */);
    handler_->network_configuration_handler()
        ->CreateConfiguration(*shill_dictionary,
                              base::Bind(&IgnoreString),
                              base::Bind(&LogErrorWithDict, FROM_HERE));
  }
}

}  // namespace chromeos

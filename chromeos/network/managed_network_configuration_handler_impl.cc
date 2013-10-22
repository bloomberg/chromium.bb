// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler_impl.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_validator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

typedef std::map<std::string, const base::DictionaryValue*> GuidToPolicyMap;

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kInvalidUserSettings[] = "InvalidUserSettings";
const char kNetworkAlreadyConfigured[] = "NetworkAlreadyConfigured";
const char kPoliciesNotInitialized[] = "PoliciesNotInitialized";
const char kProfileNotInitialized[] = "ProflieNotInitialized";
const char kSetOnUnconfiguredNetwork[] = "SetCalledOnUnconfiguredNetwork";
const char kUnknownProfilePath[] = "UnknownProfilePath";
const char kUnknownServicePath[] = "UnknownServicePath";

std::string ToDebugString(::onc::ONCSource source,
                          const std::string& userhash) {
  return source == ::onc::ONC_SOURCE_USER_POLICY ?
      ("user policy of " + userhash) : "device policy";
}

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "ManagedConfig Error: " + error_name;
  NET_LOG_ERROR(error_msg, service_path);
  network_handler::RunErrorCallback(
      error_callback, service_path, error_name, error_msg);
}

void LogErrorWithDict(const tracked_objects::Location& from_where,
                      const std::string& error_name,
                      scoped_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << from_where.ToString() << ": " << error_name;
}

const base::DictionaryValue* GetByGUID(const GuidToPolicyMap& policies,
                                       const std::string& guid) {
  GuidToPolicyMap::const_iterator it = policies.find(guid);
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

struct ManagedNetworkConfigurationHandlerImpl::Policies {
  ~Policies();

  GuidToPolicyMap per_network_config;
  base::DictionaryValue global_network_config;
};

ManagedNetworkConfigurationHandlerImpl::Policies::~Policies() {
  STLDeleteValues(&per_network_config);
}

void ManagedNetworkConfigurationHandlerImpl::AddObserver(
    NetworkPolicyObserver* observer) {
  observers_.AddObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveObserver(
    NetworkPolicyObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::GetManagedProperties(
    const std::string& userhash,
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (!GetPoliciesForUser(userhash) || !GetPoliciesForUser(std::string())) {
    InvokeErrorCallback(service_path, error_callback, kPoliciesNotInitialized);
    return;
  }
  network_configuration_handler_->GetProperties(
      service_path,
      base::Bind(
          &ManagedNetworkConfigurationHandlerImpl::GetManagedPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(),
          callback,
          error_callback),
      error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::GetManagedPropertiesCallback(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string profile_path;
  shill_properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                 &profile_path);
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    LOG(ERROR) << "No or no known profile received for service "
            << service_path << ".";
  }

  scoped_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(shill_properties);

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
  active_settings->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
                                                 &guid);

  const base::DictionaryValue* user_policy = NULL;
  const base::DictionaryValue* device_policy = NULL;
  if (!guid.empty() && profile) {
    const Policies* policies = GetPoliciesForProfile(*profile);
    if (!policies) {
      InvokeErrorCallback(
          service_path, error_callback, kPoliciesNotInitialized);
      return;
    }
    const base::DictionaryValue* policy =
        GetByGUID(policies->per_network_config, guid);
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

void ManagedNetworkConfigurationHandlerImpl::GetProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  network_configuration_handler_->GetProperties(
      service_path,
      base::Bind(&TranslatePropertiesToOncAndRunCallback, callback),
      error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& user_settings,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  const NetworkState* state =
      network_state_handler_->GetNetworkState(service_path);

  if (!state) {
    InvokeErrorCallback(service_path, error_callback, kUnknownServicePath);
    return;
  }

  std::string guid = state->guid();
  if (guid.empty()) {
    // TODO(pneubeck): create an initial configuration in this case. As for
    // CreateConfiguration, user settings from older ChromeOS versions have to
    // determined here.
    InvokeErrorCallback(
        service_path, error_callback, kSetOnUnconfiguredNetwork);
    return;
  }

  const std::string& profile_path = state->profile_path();
  const NetworkProfile *profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    InvokeErrorCallback(service_path, error_callback, kUnknownProfilePath);
    return;
  }

  VLOG(2) << "SetProperties: Found GUID " << guid << " and profile "
          << profile->ToDebugString();

  const Policies* policies = GetPoliciesForProfile(*profile);
  if (!policies) {
    InvokeErrorCallback(service_path, error_callback, kPoliciesNotInitialized);
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
    InvokeErrorCallback(service_path, error_callback, kInvalidUserSettings);
    return;
  }
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    LOG(WARNING) << "Validation of ONC user settings produced warnings.";

  const base::DictionaryValue* policy =
      GetByGUID(policies->per_network_config, guid);
  VLOG(2) << "This configuration is " << (policy ? "" : "not ") << "managed.";

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(
          *profile, guid, policy, validated_user_settings.get()));

  network_configuration_handler_->SetProperties(
      service_path, *shill_dictionary, callback, error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfiguration(
    const std::string& userhash,
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  const Policies* policies = GetPoliciesForUser(userhash);
  if (!policies) {
    InvokeErrorCallback("", error_callback, kPoliciesNotInitialized);
    return;
  }

  if (policy_util::FindMatchingPolicy(policies->per_network_config,
                                      properties)) {
    InvokeErrorCallback("", error_callback, kNetworkAlreadyConfigured);
    return;
  }

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    InvokeErrorCallback("", error_callback, kProfileNotInitialized);
    return;
  }

  // TODO(pneubeck): In case of WiFi, check that no other configuration for the
  // same {SSID, mode, security} exists. We don't support such multiple
  // configurations, yet.

  // Generate a new GUID for this configuration. Ignore the maybe provided GUID
  // in |properties| as it is not our own and from an untrusted source.
  std::string guid = base::GenerateGUID();
  scoped_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(
          *profile, guid, NULL /*no policy*/, &properties));

  network_configuration_handler_->CreateConfiguration(
      *shill_dictionary, callback, error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  network_configuration_handler_->RemoveConfiguration(
      service_path, callback, error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::SetPolicy(
    ::onc::ONCSource onc_source,
    const std::string& userhash,
    const base::ListValue& network_configs_onc,
    const base::DictionaryValue& global_network_config) {
  VLOG(1) << "Setting policies from " << ToDebugString(onc_source, userhash)
          << ".";

  // |userhash| must be empty for device policies.
  DCHECK(onc_source != ::onc::ONC_SOURCE_DEVICE_POLICY ||
         userhash.empty());
  Policies* policies = NULL;
  if (ContainsKey(policies_by_user_, userhash)) {
    policies = policies_by_user_[userhash].get();
  } else {
    policies = new Policies;
    policies_by_user_[userhash] = make_linked_ptr(policies);
  }

  policies->global_network_config.MergeDictionary(&global_network_config);

  GuidToPolicyMap old_per_network_config;
  policies->per_network_config.swap(old_per_network_config);

  // This stores all GUIDs of policies that have changed or are new.
  std::set<std::string> modified_policies;

  for (base::ListValue::const_iterator it = network_configs_onc.begin();
       it != network_configs_onc.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    std::string guid;
    network->GetStringWithoutPathExpansion(::onc::network_config::kGUID, &guid);
    DCHECK(!guid.empty());

    if (policies->per_network_config.count(guid) > 0) {
      LOG(ERROR) << "ONC from " << ToDebugString(onc_source, userhash)
                 << " contains several entries for the same GUID "
                 << guid << ".";
      delete policies->per_network_config[guid];
    }
    const base::DictionaryValue* new_entry = network->DeepCopy();
    policies->per_network_config[guid] = new_entry;

    const base::DictionaryValue* old_entry = old_per_network_config[guid];
    if (!old_entry || !old_entry->Equals(new_entry))
      modified_policies.insert(guid);
  }

  STLDeleteValues(&old_per_network_config);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    VLOG(1) << "The relevant Shill profile isn't initialized yet, postponing "
            << "policy application.";
    return;
  }

  scoped_refptr<PolicyApplicator> applicator =
      new PolicyApplicator(weak_ptr_factory_.GetWeakPtr(),
                           *profile,
                           policies->per_network_config,
                           policies->global_network_config,
                           &modified_policies);
  applicator->Run();
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileAdded(
    const NetworkProfile& profile) {
  VLOG(1) << "Adding profile " << profile.ToDebugString() << "'.";

  const Policies* policies = GetPoliciesForProfile(profile);
  if (!policies) {
    VLOG(1) << "The relevant policy is not initialized, "
            << "postponing policy application.";
    return;
  }

  std::set<std::string> policy_guids;
  for (GuidToPolicyMap::const_iterator it =
           policies->per_network_config.begin();
       it != policies->per_network_config.end(); ++it) {
    policy_guids.insert(it->first);
  }

  scoped_refptr<PolicyApplicator> applicator =
      new PolicyApplicator(weak_ptr_factory_.GetWeakPtr(),
                           profile,
                           policies->per_network_config,
                           policies->global_network_config,
                           &policy_guids);
  applicator->Run();
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileRemoved(
    const NetworkProfile& profile) {
  // Nothing to do in this case.
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfigurationFromPolicy(
    const base::DictionaryValue& shill_properties) {
  network_configuration_handler_->CreateConfiguration(
      shill_properties,
      base::Bind(&ManagedNetworkConfigurationHandlerImpl::OnPolicyApplied,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&LogErrorWithDict, FROM_HERE));
}

void ManagedNetworkConfigurationHandlerImpl::
    UpdateExistingConfigurationWithPropertiesFromPolicy(
        const base::DictionaryValue& existing_properties,
        const base::DictionaryValue& new_properties) {
  base::DictionaryValue shill_properties;

  std::string profile;
  existing_properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                    &profile);
  if (profile.empty()) {
    LOG(ERROR) << "Missing profile property.";
    return;
  }
  shill_properties.SetStringWithoutPathExpansion(shill::kProfileProperty,
                                                 profile);

  if (!shill_property_util::CopyIdentifyingProperties(existing_properties,
                                                      &shill_properties)) {
    LOG(ERROR) << "Missing identifying properties.";
  }

  shill_properties.MergeDictionary(&new_properties);

  network_configuration_handler_->CreateConfiguration(
      shill_properties,
      base::Bind(&ManagedNetworkConfigurationHandlerImpl::OnPolicyApplied,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&LogErrorWithDict, FROM_HERE));
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGUID(
    const std::string userhash,
    const std::string& guid,
    ::onc::ONCSource* onc_source) const {
  *onc_source = ::onc::ONC_SOURCE_NONE;

  if (!userhash.empty()) {
    const Policies* user_policies = GetPoliciesForUser(userhash);
    if (user_policies) {
      const base::DictionaryValue* policy =
          GetByGUID(user_policies->per_network_config, guid);
      if (policy) {
        *onc_source = ::onc::ONC_SOURCE_USER_POLICY;
        return policy;
      }
    }
  }

  const Policies* device_policies = GetPoliciesForUser(std::string());
  if (device_policies) {
    const base::DictionaryValue* policy =
        GetByGUID(device_policies->per_network_config, guid);
    if (policy) {
      *onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
      return policy;
    }
  }

  return NULL;
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::GetGlobalConfigFromPolicy(
    const std::string userhash) const {
  const Policies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return NULL;

  return &policies->global_network_config;
}
const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGuidAndProfile(
    const std::string& guid,
    const std::string& profile_path) const {
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    LOG(ERROR) << "Profile path unknown: " << profile_path;
    return NULL;
  }

  const Policies* policies = GetPoliciesForProfile(*profile);
  if (!policies)
    return NULL;

  return GetByGUID(policies->per_network_config, guid);
}

const ManagedNetworkConfigurationHandlerImpl::Policies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForUser(
    const std::string& userhash) const {
  UserToPoliciesMap::const_iterator it = policies_by_user_.find(userhash);
  if (it == policies_by_user_.end())
    return NULL;
  return it->second.get();
}

const ManagedNetworkConfigurationHandlerImpl::Policies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForProfile(
    const NetworkProfile& profile) const {
  DCHECK(profile.type() != NetworkProfile::TYPE_SHARED ||
         profile.userhash.empty());
  return GetPoliciesForUser(profile.userhash);
}

ManagedNetworkConfigurationHandlerImpl::ManagedNetworkConfigurationHandlerImpl()
    : network_state_handler_(NULL),
      network_profile_handler_(NULL),
      network_configuration_handler_(NULL),
      weak_ptr_factory_(this) {}

ManagedNetworkConfigurationHandlerImpl::
    ~ManagedNetworkConfigurationHandlerImpl() {
  network_profile_handler_->RemoveObserver(this);
}

void ManagedNetworkConfigurationHandlerImpl::Init(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler) {
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_profile_handler_->AddObserver(this);
}

void ManagedNetworkConfigurationHandlerImpl::OnPolicyApplied(
    const std::string& service_path) {
  if (service_path.empty())
    return;
  FOR_EACH_OBSERVER(
      NetworkPolicyObserver, observers_, PolicyApplied(service_path));
}

}  // namespace chromeos

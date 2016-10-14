// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/prohibited_technologies_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

using GuidToPolicyMap = ManagedNetworkConfigurationHandler::GuidToPolicyMap;

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kInvalidUserSettings[] = "InvalidUserSettings";
const char kNetworkAlreadyConfigured[] = "NetworkAlreadyConfigured";
const char kPoliciesNotInitialized[] = "PoliciesNotInitialized";
const char kProfileNotInitialized[] = "ProfileNotInitialized";
const char kUnconfiguredNetwork[] = "UnconfiguredNetwork";
const char kUnknownNetwork[] = "UnknownNetwork";

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
                      std::unique_ptr<base::DictionaryValue> error_data) {
  device_event_log::AddEntry(from_where.file_name(), from_where.line_number(),
                             device_event_log::LOG_TYPE_NETWORK,
                             device_event_log::LOG_LEVEL_ERROR, error_name);
}

const base::DictionaryValue* GetByGUID(const GuidToPolicyMap& policies,
                                       const std::string& guid) {
  auto it = policies.find(guid);
  if (it == policies.end())
    return NULL;
  return it->second.get();
}

}  // namespace

struct ManagedNetworkConfigurationHandlerImpl::Policies {
  ~Policies();

  GuidToPolicyMap per_network_config;
  base::DictionaryValue global_network_config;
};

ManagedNetworkConfigurationHandlerImpl::Policies::~Policies() {}

void ManagedNetworkConfigurationHandlerImpl::AddObserver(
    NetworkPolicyObserver* observer) {
  observers_.AddObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveObserver(
    NetworkPolicyObserver* observer) {
  observers_.RemoveObserver(observer);
}

// GetManagedProperties

void ManagedNetworkConfigurationHandlerImpl::GetManagedProperties(
    const std::string& userhash,
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (!GetPoliciesForUser(userhash) || !GetPoliciesForUser(std::string())) {
    InvokeErrorCallback(service_path, error_callback, kPoliciesNotInitialized);
    return;
  }
  NET_LOG_USER("GetManagedProperties", service_path);
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(
          &ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(),
          base::Bind(
              &ManagedNetworkConfigurationHandlerImpl::SendManagedProperties,
              weak_ptr_factory_.GetWeakPtr(), userhash, callback,
              error_callback)),
      error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::SendManagedProperties(
    const std::string& userhash,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> shill_properties) {
  std::string profile_path;
  shill_properties->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                  &profile_path);
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile)
    NET_LOG_ERROR("No profile for service: " + profile_path, service_path);

  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(*shill_properties);

  const base::DictionaryValue* user_settings = NULL;

  if (ui_data && profile) {
    user_settings = ui_data->user_settings();
  } else if (profile) {
    NET_LOG_DEBUG("Service contains empty or invalid UIData", service_path);
    // TODO(pneubeck): add a conversion of user configured entries of old
    // ChromeOS versions. We will have to use a heuristic to determine which
    // properties _might_ be user configured.
  }

  std::string guid;
  shill_properties->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);

  ::onc::ONCSource onc_source;
  FindPolicyByGUID(userhash, guid, &onc_source);
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  std::unique_ptr<base::DictionaryValue> active_settings(
      onc::TranslateShillServiceToONCPart(*shill_properties, onc_source,
                                          &onc::kNetworkWithStateSignature,
                                          network_state));

  const base::DictionaryValue* network_policy = NULL;
  const base::DictionaryValue* global_policy = NULL;
  if (profile) {
    const Policies* policies = GetPoliciesForProfile(*profile);
    if (!policies) {
      InvokeErrorCallback(
          service_path, error_callback, kPoliciesNotInitialized);
      return;
    }
    if (!guid.empty())
      network_policy = GetByGUID(policies->per_network_config, guid);
    global_policy = &policies->global_network_config;
  }

  std::unique_ptr<base::DictionaryValue> augmented_properties(
      policy_util::CreateManagedONC(global_policy, network_policy,
                                    user_settings, active_settings.get(),
                                    profile));
  callback.Run(service_path, *augmented_properties);
}

// GetProperties

void ManagedNetworkConfigurationHandlerImpl::GetProperties(
    const std::string& userhash,
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("GetProperties", service_path);
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(
          &ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(),
          base::Bind(&ManagedNetworkConfigurationHandlerImpl::SendProperties,
                     weak_ptr_factory_.GetWeakPtr(), userhash, callback,
                     error_callback)),
      error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::SendProperties(
    const std::string& userhash,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> shill_properties) {
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);

  std::string guid;
  shill_properties->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);

  ::onc::ONCSource onc_source;
  FindPolicyByGUID(userhash, guid, &onc_source);

  std::unique_ptr<base::DictionaryValue> onc_network(
      onc::TranslateShillServiceToONCPart(*shill_properties, onc_source,
                                          &onc::kNetworkWithStateSignature,
                                          network_state));
  callback.Run(service_path, *onc_network);
}

// SetProperties

void ManagedNetworkConfigurationHandlerImpl::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& user_settings,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  const NetworkState* state =
      network_state_handler_->GetNetworkStateFromServicePath(
          service_path, true /* configured_only */);
  if (!state) {
    InvokeErrorCallback(service_path, error_callback, kUnknownNetwork);
    return;
  }

  std::string guid = state->guid();
  DCHECK(!guid.empty());

  const std::string& profile_path = state->profile_path();
  const NetworkProfile *profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    // TODO(pneubeck): create an initial configuration in this case. As for
    // CreateConfiguration, user settings from older ChromeOS versions have to
    // be determined here.
    InvokeErrorCallback(service_path, error_callback, kUnconfiguredNetwork);
    return;
  }

  VLOG(2) << "SetProperties: Found GUID " << guid << " and profile "
          << profile->ToDebugString();

  const Policies* policies = GetPoliciesForProfile(*profile);
  if (!policies) {
    InvokeErrorCallback(service_path, error_callback, kPoliciesNotInitialized);
    return;
  }

  // We need to ensure that required configuration properties (e.g. Type) are
  // included for ONC validation and translation to Shill properties.
  std::unique_ptr<base::DictionaryValue> user_settings_copy(
      user_settings.DeepCopy());
  user_settings_copy->SetStringWithoutPathExpansion(
      ::onc::network_config::kType,
      network_util::TranslateShillTypeToONC(state->type()));
  user_settings_copy->MergeDictionary(&user_settings);

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  onc::Validator validator(false,  // Ignore unknown fields.
                           false,  // Ignore invalid recommended field names.
                           false,  // Ignore missing fields.
                           false);  // This ONC does not come from policy.

  onc::Validator::Result validation_result;
  std::unique_ptr<base::DictionaryValue> validated_user_settings =
      validator.ValidateAndRepairObject(&onc::kNetworkConfigurationSignature,
                                        *user_settings_copy,
                                        &validation_result);
  if (validation_result == onc::Validator::INVALID) {
    InvokeErrorCallback(service_path, error_callback, kInvalidUserSettings);
    return;
  }
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    LOG(WARNING) << "Validation of ONC user settings produced warnings.";

  // Fill in HexSSID field from contents of SSID field if not set already.
  if (user_settings_copy) {
    onc::FillInHexSSIDFieldsInOncObject(onc::kNetworkConfigurationSignature,
                                        validated_user_settings.get());
  }

  const base::DictionaryValue* network_policy =
      GetByGUID(policies->per_network_config, guid);
  VLOG(2) << "This configuration is " << (network_policy ? "" : "not ")
          << "managed.";

  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(
          *profile, guid, &policies->global_network_config, network_policy,
          validated_user_settings.get()));

  // 'Carrier' needs to be handled specially if set.
  base::DictionaryValue* cellular = nullptr;
  if (validated_user_settings->GetDictionaryWithoutPathExpansion(
          ::onc::network_config::kCellular, &cellular)) {
    std::string carrier;
    if (cellular->GetStringWithoutPathExpansion(::onc::cellular::kCarrier,
                                                &carrier)) {
      network_device_handler_->SetCarrier(
          state->device_path(), carrier,
          base::Bind(
              &ManagedNetworkConfigurationHandlerImpl::SetShillProperties,
              weak_ptr_factory_.GetWeakPtr(), service_path,
              base::Passed(&shill_dictionary), callback, error_callback),
          error_callback);
      return;
    }
  }

  SetShillProperties(service_path, std::move(shill_dictionary), callback,
                     error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::SetShillProperties(
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> shill_dictionary,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  network_configuration_handler_->SetShillProperties(
      service_path, *shill_dictionary,
      NetworkConfigurationObserver::SOURCE_USER_ACTION, callback,
      error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfiguration(
    const std::string& userhash,
    const base::DictionaryValue& properties,
    const network_handler::ServiceResultCallback& callback,
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
  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(*profile, guid,
                                            NULL,  // no global policy
                                            NULL,  // no network policy
                                            &properties));

  network_configuration_handler_->CreateShillConfiguration(
      *shill_dictionary, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      callback, error_callback);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  network_configuration_handler_->RemoveConfiguration(
      service_path, NetworkConfigurationObserver::SOURCE_USER_ACTION, callback,
      error_callback);
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
  if (base::ContainsKey(policies_by_user_, userhash)) {
    policies = policies_by_user_[userhash].get();
  } else {
    policies = new Policies;
    policies_by_user_[userhash] = base::WrapUnique(policies);
  }

  policies->global_network_config.MergeDictionary(&global_network_config);

  // Update prohibited technologies.
  const base::ListValue* prohibited_list = nullptr;
  if (policies->global_network_config.GetListWithoutPathExpansion(
          ::onc::global_network_config::kDisableNetworkTypes,
          &prohibited_list) &&
      prohibited_technologies_handler_) {
    // Prohibited technologies are only allowed in user policy.
    DCHECK_EQ(::onc::ONC_SOURCE_DEVICE_POLICY, onc_source);

    prohibited_technologies_handler_->SetProhibitedTechnologies(
        prohibited_list);
  }

  GuidToPolicyMap old_per_network_config;
  policies->per_network_config.swap(old_per_network_config);

  // This stores all GUIDs of policies that have changed or are new.
  std::set<std::string> modified_policies;

  for (base::ListValue::const_iterator it = network_configs_onc.begin();
       it != network_configs_onc.end(); ++it) {
    base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    std::string guid;
    network->GetStringWithoutPathExpansion(::onc::network_config::kGUID, &guid);
    DCHECK(!guid.empty());

    if (policies->per_network_config.count(guid) > 0) {
      NET_LOG_ERROR("ONC from " + ToDebugString(onc_source, userhash) +
                    " contains several entries for the same GUID ", guid);
    }
    base::DictionaryValue* new_entry = network->DeepCopy();
    policies->per_network_config[guid] = base::WrapUnique(new_entry);

    base::DictionaryValue* old_entry = old_per_network_config[guid].get();
    if (!old_entry || !old_entry->Equals(new_entry))
      modified_policies.insert(guid);
  }

  old_per_network_config.clear();
  ApplyOrQueuePolicies(userhash, &modified_policies);
  for (auto& observer : observers_)
    observer.PoliciesChanged(userhash);
}

bool ManagedNetworkConfigurationHandlerImpl::IsAnyPolicyApplicationRunning()
    const {
  return !policy_applicators_.empty() || !queued_modified_policies_.empty();
}

bool ManagedNetworkConfigurationHandlerImpl::ApplyOrQueuePolicies(
    const std::string& userhash,
    std::set<std::string>* modified_policies) {
  DCHECK(modified_policies);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    VLOG(1) << "The relevant Shill profile isn't initialized yet, postponing "
            << "policy application.";
    // OnProfileAdded will apply all policies for this userhash.
    return false;
  }

  if (base::ContainsKey(policy_applicators_, userhash)) {
    // A previous policy application is still running. Queue the modified
    // policies.
    // Note, even if |modified_policies| is empty, this means that a policy
    // application will be queued.
    queued_modified_policies_[userhash].insert(modified_policies->begin(),
                                               modified_policies->end());
    VLOG(1) << "Previous PolicyApplicator still running. Postponing policy "
               "application.";
    return false;
  }

  const Policies* policies = policies_by_user_[userhash].get();
  DCHECK(policies);

  PolicyApplicator* applicator =
      new PolicyApplicator(*profile,
                           policies->per_network_config,
                           policies->global_network_config,
                           this,
                           modified_policies);
  policy_applicators_[userhash] = base::WrapUnique(applicator);
  applicator->Run();
  return true;
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileAdded(
    const NetworkProfile& profile) {
  VLOG(1) << "Adding profile " << profile.ToDebugString() << "'.";

  const Policies* policies = GetPoliciesForProfile(profile);
  if (!policies) {
    VLOG(1) << "The relevant policy is not initialized, "
            << "postponing policy application.";
    // See SetPolicy.
    return;
  }

  std::set<std::string> policy_guids;
  for (auto it = policies->per_network_config.begin();
       it != policies->per_network_config.end(); ++it) {
    policy_guids.insert(it->first);
  }

  const bool started_policy_application =
      ApplyOrQueuePolicies(profile.userhash, &policy_guids);
  DCHECK(started_policy_application);
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileRemoved(
    const NetworkProfile& profile) {
  // Nothing to do in this case.
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfigurationFromPolicy(
    const base::DictionaryValue& shill_properties) {
  network_configuration_handler_->CreateShillConfiguration(
      shill_properties, NetworkConfigurationObserver::SOURCE_POLICY,
      base::Bind(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
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
    NET_LOG_ERROR("Missing profile property",
                  shill_property_util::GetNetworkIdFromProperties(
                      existing_properties));
    return;
  }
  shill_properties.SetStringWithoutPathExpansion(shill::kProfileProperty,
                                                 profile);

  if (!shill_property_util::CopyIdentifyingProperties(
          existing_properties,
          true /* properties were read from Shill */,
          &shill_properties)) {
    NET_LOG_ERROR("Missing identifying properties",
                  shill_property_util::GetNetworkIdFromProperties(
                      existing_properties));
  }

  shill_properties.MergeDictionary(&new_properties);

  network_configuration_handler_->CreateShillConfiguration(
      shill_properties, NetworkConfigurationObserver::SOURCE_POLICY,
      base::Bind(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&LogErrorWithDict, FROM_HERE));
}

void ManagedNetworkConfigurationHandlerImpl::OnPoliciesApplied(
    const NetworkProfile& profile) {
  const std::string& userhash = profile.userhash;
  VLOG(1) << "Policy application for user '" << userhash << "' finished.";

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, policy_applicators_[userhash].release());
  policy_applicators_.erase(userhash);

  if (base::ContainsKey(queued_modified_policies_, userhash)) {
    std::set<std::string> modified_policies;
    queued_modified_policies_[userhash].swap(modified_policies);
    // Remove |userhash| from the queue.
    queued_modified_policies_.erase(userhash);
    ApplyOrQueuePolicies(userhash, &modified_policies);
  } else {
    for (auto& observer : observers_)
      observer.PoliciesApplied(userhash);
  }
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

const GuidToPolicyMap*
ManagedNetworkConfigurationHandlerImpl::GetNetworkConfigsFromPolicy(
    const std::string& userhash) const {
  const Policies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return NULL;

  return &policies->per_network_config;
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::GetGlobalConfigFromPolicy(
    const std::string& userhash) const {
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
    NET_LOG_ERROR("Profile path unknown:" + profile_path, guid);
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
      network_device_handler_(NULL),
      weak_ptr_factory_(this) {
  CHECK(base::ThreadTaskRunnerHandle::IsSet());
}

ManagedNetworkConfigurationHandlerImpl::
    ~ManagedNetworkConfigurationHandlerImpl() {
  if (network_profile_handler_)
    network_profile_handler_->RemoveObserver(this);
}

void ManagedNetworkConfigurationHandlerImpl::Init(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkDeviceHandler* network_device_handler,
    ProhibitedTechnologiesHandler* prohibited_technologies_handler) {
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_device_handler_ = network_device_handler;
  network_profile_handler_->AddObserver(this);
  prohibited_technologies_handler_ = prohibited_technologies_handler;
}

void ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork(
    const std::string& service_path,
    const std::string& guid) {
  if (service_path.empty())
    return;
  for (auto& observer : observers_)
    observer.PolicyAppliedToNetwork(service_path);
}

// Get{Managed}Properties helpers

void ManagedNetworkConfigurationHandlerImpl::GetDeviceStateProperties(
    const std::string& service_path,
    base::DictionaryValue* properties) {
  std::string connection_state;
  properties->GetStringWithoutPathExpansion(
      shill::kStateProperty, &connection_state);
  if (!NetworkState::StateIsConnected(connection_state))
    return;

  // Get the IPConfig properties from the device and store them in "IPConfigs"
  // (plural) in the properties dictionary. (Note: Shill only provides a single
  // "IPConfig" property for a network service, but a consumer of this API may
  // want information about all ipv4 and ipv6 IPConfig properties.
  std::string device;
  properties->GetStringWithoutPathExpansion(shill::kDeviceProperty, &device);
  const DeviceState* device_state =
      network_state_handler_->GetDeviceState(device);
  if (!device_state) {
    NET_LOG_ERROR("GetDeviceProperties: no device: " + device, service_path);
    return;
  }

  // Get the hardware MAC address from the DeviceState.
  if (!device_state->mac_address().empty()) {
    properties->SetStringWithoutPathExpansion(
        shill::kAddressProperty, device_state->mac_address());
  }

  // Convert IPConfig dictionary to a ListValue.
  base::ListValue* ip_configs = new base::ListValue;
  for (base::DictionaryValue::Iterator iter(device_state->ip_configs());
       !iter.IsAtEnd(); iter.Advance()) {
    ip_configs->Append(iter.value().CreateDeepCopy());
  }
  properties->SetWithoutPathExpansion(shill::kIPConfigsProperty, ip_configs);
}

void ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback(
    GetDevicePropertiesCallback send_callback,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::unique_ptr<base::DictionaryValue> shill_properties_copy(
      shill_properties.DeepCopy());

  std::string guid;
  shill_properties.GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  if (guid.empty()) {
    // Unmanaged networks are assigned a GUID in NetworkState. Provide this
    // value in the ONC dictionary.
    const NetworkState* state =
        network_state_handler_->GetNetworkState(service_path);
    if (state && !state->guid().empty()) {
      guid = state->guid();
      shill_properties_copy->SetStringWithoutPathExpansion(shill::kGuidProperty,
                                                           guid);
    } else {
      LOG(ERROR) << "Network has no GUID specified: " << service_path;
    }
  }

  std::string type;
  shill_properties_copy->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                       &type);
  // Add associated DeviceState properties for non-VPN networks.
  if (type != shill::kTypeVPN)
    GetDeviceStateProperties(service_path, shill_properties_copy.get());

  // Only request additional Device properties for Cellular networks with a
  // valid device.
  std::string device_path;
  if (!network_device_handler_ ||
      type != shill::kTypeCellular ||
      !shill_properties_copy->GetStringWithoutPathExpansion(
          shill::kDeviceProperty, &device_path) ||
      device_path.empty()) {
    send_callback.Run(service_path, std::move(shill_properties_copy));
    return;
  }

  // Request the device properties. On success or failure pass (a possibly
  // modified) |shill_properties| to |send_callback|.
  std::unique_ptr<base::DictionaryValue> shill_properties_copy_error_copy(
      shill_properties_copy->DeepCopy());
  network_device_handler_->GetDeviceProperties(
      device_path,
      base::Bind(&ManagedNetworkConfigurationHandlerImpl::
                     GetDevicePropertiesSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_path,
                 base::Passed(&shill_properties_copy),
                 send_callback),
      base::Bind(&ManagedNetworkConfigurationHandlerImpl::
                     GetDevicePropertiesFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 service_path,
                 base::Passed(&shill_properties_copy_error_copy),
                 send_callback));
}

void ManagedNetworkConfigurationHandlerImpl::GetDevicePropertiesSuccess(
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> network_properties,
    GetDevicePropertiesCallback send_callback,
    const std::string& device_path,
    const base::DictionaryValue& device_properties) {
  // Create a "Device" dictionary in |network_properties|.
  network_properties->SetWithoutPathExpansion(
      shill::kDeviceProperty, device_properties.DeepCopy());
  send_callback.Run(service_path, std::move(network_properties));
}

void ManagedNetworkConfigurationHandlerImpl::GetDevicePropertiesFailure(
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> network_properties,
    GetDevicePropertiesCallback send_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Error getting device properties", service_path);
  send_callback.Run(service_path, std::move(network_properties));
}


}  // namespace chromeos

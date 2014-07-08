// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/policy_applicator.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void LogErrorMessage(const tracked_objects::Location& from_where,
                     const std::string& error_name,
                     const std::string& error_message) {
  LOG(ERROR) << from_where.ToString() << ": " << error_message;
}

const base::DictionaryValue* GetByGUID(
    const PolicyApplicator::GuidToPolicyMap& policies,
    const std::string& guid) {
  PolicyApplicator::GuidToPolicyMap::const_iterator it = policies.find(guid);
  if (it == policies.end())
    return NULL;
  return it->second;
}

}  // namespace

PolicyApplicator::PolicyApplicator(
    base::WeakPtr<ConfigurationHandler> handler,
    const NetworkProfile& profile,
    const GuidToPolicyMap& all_policies,
    const base::DictionaryValue& global_network_config,
    std::set<std::string>* modified_policies)
    : handler_(handler), profile_(profile) {
  global_network_config_.MergeDictionary(&global_network_config);
  remaining_policies_.swap(*modified_policies);
  for (GuidToPolicyMap::const_iterator it = all_policies.begin();
       it != all_policies.end(); ++it) {
    all_policies_.insert(std::make_pair(it->first, it->second->DeepCopy()));
  }
}

void PolicyApplicator::Run() {
  DBusThreadManager::Get()->GetShillProfileClient()->GetProperties(
      dbus::ObjectPath(profile_.path),
      base::Bind(&PolicyApplicator::GetProfilePropertiesCallback, this),
      base::Bind(&LogErrorMessage, FROM_HERE));
}

void PolicyApplicator::GetProfilePropertiesCallback(
    const base::DictionaryValue& profile_properties) {
  if (!handler_) {
    LOG(WARNING) << "Handler destructed during policy application to profile "
                 << profile_.ToDebugString();
    return;
  }

  VLOG(2) << "Received properties for profile " << profile_.ToDebugString();
  const base::ListValue* entries = NULL;
  if (!profile_properties.GetListWithoutPathExpansion(
           shill::kEntriesProperty, &entries)) {
    LOG(ERROR) << "Profile " << profile_.ToDebugString()
               << " doesn't contain the property "
               << shill::kEntriesProperty;
    return;
  }

  for (base::ListValue::const_iterator it = entries->begin();
       it != entries->end(); ++it) {
    std::string entry;
    (*it)->GetAsString(&entry);

    DBusThreadManager::Get()->GetShillProfileClient()->GetEntry(
        dbus::ObjectPath(profile_.path),
        entry,
        base::Bind(&PolicyApplicator::GetEntryCallback, this, entry),
        base::Bind(&LogErrorMessage, FROM_HERE));
  }
}

void PolicyApplicator::GetEntryCallback(
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
  if (!onc_part->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
                                               &old_guid)) {
    VLOG(1) << "Entry " << entry << " of profile " << profile_.ToDebugString()
            << " doesn't contain a GUID.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged.
  }

  scoped_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(entry_properties);
  if (!ui_data) {
    VLOG(1) << "Entry " << entry << " of profile " << profile_.ToDebugString()
            << " contains no or no valid UIData.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged. It's an inconsistency if there is a GUID but no UIData, thus
    // clear the GUID just in case.
    old_guid.clear();
  }

  bool was_managed = !old_guid.empty() && ui_data &&
                     (ui_data->onc_source() ==
                          ::onc::ONC_SOURCE_DEVICE_POLICY ||
                      ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);

  const base::DictionaryValue* new_policy = NULL;
  if (was_managed) {
    // If we have a GUID that might match a current policy, do a lookup using
    // that GUID at first. In particular this is necessary, as some networks
    // can't be matched to policies by properties (e.g. VPN).
    new_policy = GetByGUID(all_policies_, old_guid);
  }

  if (!new_policy) {
    // If we didn't find a policy by GUID, still a new policy might match.
    new_policy = policy_util::FindMatchingPolicy(all_policies_, *onc_part);
  }

  if (new_policy) {
    std::string new_guid;
    new_policy->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
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
      const base::DictionaryValue* user_settings =
          ui_data ? ui_data->user_settings() : NULL;
      scoped_ptr<base::DictionaryValue> new_shill_properties =
          policy_util::CreateShillConfiguration(
              profile_, new_guid, new_policy, user_settings);
      // A new policy has to be applied to this profile entry. In order to keep
      // implicit state of Shill like "connected successfully before", keep the
      // entry if a policy is reapplied (e.g. after reboot) or is updated.
      // However, some Shill properties are used to identify the network and
      // cannot be modified after initial configuration, so we have to delete
      // the profile entry in these cases. Also, keeping Shill's state if the
      // SSID changed might not be a good idea anyways. If the policy GUID
      // changed, or there was no policy before, we delete the entry at first to
      // ensure that no old configuration remains.
      if (old_guid == new_guid &&
          shill_property_util::DoIdentifyingPropertiesMatch(
              *new_shill_properties, entry_properties)) {
        VLOG(1) << "Updating previously managed configuration with the "
                << "updated policy " << new_guid << ".";
      } else {
        VLOG(1) << "Deleting profile entry before writing new policy "
                << new_guid << " because of identifying properties changed.";
        DeleteEntry(entry);
      }

      // In general, old entries should at first be deleted before new
      // configurations are written to prevent inconsistencies. Therefore, we
      // delay the writing of the new config here until ~PolicyApplicator.
      // E.g. one problematic case is if a policy { {GUID=X, SSID=Y} } is
      // applied to the profile entries
      // { ENTRY1 = {GUID=X, SSID=X, USER_SETTINGS=X},
      //   ENTRY2 = {SSID=Y, ... } }.
      // At first ENTRY1 and ENTRY2 should be removed, then the new config be
      // written and the result should be:
      // { {GUID=X, SSID=Y, USER_SETTINGS=X} }
      WriteNewShillConfiguration(*new_shill_properties, *new_policy, true);
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
    // The entry wasn't managed and doesn't match any current policy. Global
    // network settings have to be applied.
    base::DictionaryValue shill_properties_to_update;
    GetPropertiesForUnmanagedEntry(entry_properties,
                                   &shill_properties_to_update);
    if (shill_properties_to_update.empty()) {
      VLOG(2) << "Ignore unmanaged entry.";
      // Calling a SetProperties of Shill with an empty dictionary is a no op.
    } else {
      VLOG(2) << "Apply global network config to unmanaged entry.";
      handler_->UpdateExistingConfigurationWithPropertiesFromPolicy(
          entry_properties, shill_properties_to_update);
    }
  }
}

void PolicyApplicator::DeleteEntry(const std::string& entry) {
  DBusThreadManager::Get()->GetShillProfileClient()->DeleteEntry(
      dbus::ObjectPath(profile_.path),
      entry,
      base::Bind(&base::DoNothing),
      base::Bind(&LogErrorMessage, FROM_HERE));
}

void PolicyApplicator::WriteNewShillConfiguration(
    const base::DictionaryValue& shill_dictionary,
    const base::DictionaryValue& policy,
    bool write_later) {
  // Ethernet (non EAP) settings, like GUID or UIData, cannot be stored per
  // user. Abort in that case.
  std::string type;
  policy.GetStringWithoutPathExpansion(::onc::network_config::kType, &type);
  if (type == ::onc::network_type::kEthernet &&
      profile_.type() == NetworkProfile::TYPE_USER) {
    const base::DictionaryValue* ethernet = NULL;
    policy.GetDictionaryWithoutPathExpansion(::onc::network_config::kEthernet,
                                             &ethernet);
    std::string auth;
    ethernet->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                            &auth);
    if (auth == ::onc::ethernet::kAuthenticationNone)
      return;
  }

  if (write_later)
    new_shill_configurations_.push_back(shill_dictionary.DeepCopy());
  else
    handler_->CreateConfigurationFromPolicy(shill_dictionary);
}

void PolicyApplicator::GetPropertiesForUnmanagedEntry(
    const base::DictionaryValue& entry_properties,
    base::DictionaryValue* properties_to_update) const {
  // kAllowOnlyPolicyNetworksToAutoconnect is currently the only global config.

  std::string type;
  entry_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  if (NetworkTypePattern::Ethernet().MatchesType(type))
    return;  // Autoconnect for Ethernet cannot be configured.

  // By default all networks are allowed to autoconnect.
  bool only_policy_autoconnect = false;
  global_network_config_.GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  if (!only_policy_autoconnect)
    return;

  bool old_autoconnect = false;
  if (entry_properties.GetBooleanWithoutPathExpansion(
          shill::kAutoConnectProperty, &old_autoconnect) &&
      !old_autoconnect) {
    // Autoconnect is already explictly disabled. No need to set it again.
    return;
  }
  // If autconnect is not explicitly set yet, it might automatically be enabled
  // by Shill. To prevent that, disable it explicitly.
  properties_to_update->SetBooleanWithoutPathExpansion(
      shill::kAutoConnectProperty, false);
}

PolicyApplicator::~PolicyApplicator() {
  ApplyRemainingPolicies();
  STLDeleteValues(&all_policies_);
  // Notify the handler about all policies being applied, so that the network
  // lists can be updated.
  if (handler_)
    handler_->OnPoliciesApplied();
}

void PolicyApplicator::ApplyRemainingPolicies() {
  if (!handler_) {
    LOG(WARNING) << "Handler destructed during policy application to profile "
                 << profile_.ToDebugString();
    return;
  }

  // Write all queued configurations now.
  for (ScopedVector<base::DictionaryValue>::const_iterator it =
           new_shill_configurations_.begin();
       it != new_shill_configurations_.end();
       ++it) {
    handler_->CreateConfigurationFromPolicy(**it);
  }

  if (remaining_policies_.empty())
    return;

  VLOG(2) << "Create new managed network configurations in profile"
          << profile_.ToDebugString() << ".";
  // All profile entries were compared to policies. |remaining_policies_|
  // contains all modified policies that didn't match any entry. For these
  // remaining policies, new configurations have to be created.
  for (std::set<std::string>::iterator it = remaining_policies_.begin();
       it != remaining_policies_.end(); ++it) {
    const base::DictionaryValue* policy = GetByGUID(all_policies_, *it);
    DCHECK(policy);

    VLOG(1) << "Creating new configuration managed by policy " << *it
            << " in profile " << profile_.ToDebugString() << ".";

    scoped_ptr<base::DictionaryValue> shill_dictionary =
        policy_util::CreateShillConfiguration(profile_, *it, policy, NULL);
    WriteNewShillConfiguration(*shill_dictionary, *policy, false);
  }
}

}  // namespace chromeos

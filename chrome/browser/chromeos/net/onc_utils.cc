// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/onc_utils.h"

#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"

namespace chromeos {
namespace onc {

namespace {

// This class defines which string placeholders of ONC are replaced by which
// user attribute.
class UserStringSubstitution : public chromeos::onc::StringSubstitution {
 public:
  explicit UserStringSubstitution(const user_manager::User* user)
      : user_(user) {}
  ~UserStringSubstitution() override {}

  bool GetSubstitute(const std::string& placeholder,
                     std::string* substitute) const override {
    if (placeholder == ::onc::substitutes::kLoginIDField)
      *substitute = user_->GetAccountName(false);
    else if (placeholder == ::onc::substitutes::kEmailField)
      *substitute = user_->email();
    else
      return false;
    return true;
  }

 private:
  const user_manager::User* user_;

  DISALLOW_COPY_AND_ASSIGN(UserStringSubstitution);
};

}  // namespace

void ExpandStringPlaceholdersInNetworksForUser(
    const user_manager::User* user,
    base::ListValue* network_configs) {
  if (!user) {
    // In tests no user may be logged in. It's not harmful if we just don't
    // expand the strings.
    return;
  }
  UserStringSubstitution substitution(user);
  chromeos::onc::ExpandStringsInNetworks(substitution, network_configs);
}

void ImportNetworksForUser(const user_manager::User* user,
                           const base::ListValue& network_configs,
                           std::string* error) {
  error->clear();

  scoped_ptr<base::ListValue> expanded_networks(network_configs.DeepCopy());
  ExpandStringPlaceholdersInNetworksForUser(user, expanded_networks.get());

  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetProfileForUserhash(
          user->username_hash());
  if (!profile) {
    *error = "User profile doesn't exist.";
    return;
  }

  bool ethernet_not_found = false;
  for (base::ListValue::const_iterator it = expanded_networks->begin();
       it != expanded_networks->end();
       ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    // Remove irrelevant fields.
    onc::Normalizer normalizer(true /* remove recommended fields */);
    scoped_ptr<base::DictionaryValue> normalized_network =
        normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                   *network);

    // TODO(pneubeck): Use ONC and ManagedNetworkConfigurationHandler instead.
    // crbug.com/457936
    scoped_ptr<base::DictionaryValue> shill_dict =
        onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                       *normalized_network);

    scoped_ptr<NetworkUIData> ui_data(
        NetworkUIData::CreateFromONC(::onc::ONC_SOURCE_USER_IMPORT));
    base::DictionaryValue ui_data_dict;
    ui_data->FillDictionary(&ui_data_dict);
    std::string ui_data_json;
    base::JSONWriter::Write(ui_data_dict, &ui_data_json);
    shill_dict->SetStringWithoutPathExpansion(shill::kUIDataProperty,
                                              ui_data_json);

    shill_dict->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                              profile->path);

    std::string type;
    shill_dict->GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
    NetworkConfigurationHandler* config_handler =
        NetworkHandler::Get()->network_configuration_handler();
    if (NetworkTypePattern::Ethernet().MatchesType(type)) {
      // Ethernet has to be configured using an existing Ethernet service.
      const NetworkState* ethernet =
          NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
              NetworkTypePattern::Ethernet());
      if (ethernet) {
        config_handler->SetShillProperties(
            ethernet->path(), *shill_dict,
            NetworkConfigurationObserver::SOURCE_USER_ACTION, base::Closure(),
            network_handler::ErrorCallback());
      } else {
        ethernet_not_found = true;
      }

    } else {
      config_handler->CreateShillConfiguration(
          *shill_dict, NetworkConfigurationObserver::SOURCE_USER_ACTION,
          network_handler::StringResultCallback(),
          network_handler::ErrorCallback());
    }
  }

  if (ethernet_not_found)
    *error = "No Ethernet available to configure.";
}

const base::DictionaryValue* FindPolicyForActiveUser(
    const std::string& guid,
    ::onc::ONCSource* onc_source) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  std::string username_hash = user ? user->username_hash() : std::string();
  return NetworkHandler::Get()->managed_network_configuration_handler()->
      FindPolicyByGUID(username_hash, guid, onc_source);
}

const base::DictionaryValue* GetGlobalConfigFromPolicy(bool for_active_user) {
  std::string username_hash;
  if (for_active_user) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (!user) {
      LOG(ERROR) << "No user logged in yet.";
      return NULL;
    }
    username_hash = user->username_hash();
  }
  return NetworkHandler::Get()->managed_network_configuration_handler()->
      GetGlobalConfigFromPolicy(username_hash);
}

bool PolicyAllowsOnlyPolicyNetworksToAutoconnect(bool for_active_user) {
  const base::DictionaryValue* global_config =
      GetGlobalConfigFromPolicy(for_active_user);
  if (!global_config)
    return false;  // By default, all networks are allowed to autoconnect.

  bool only_policy_autoconnect = false;
  global_config->GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  return only_policy_autoconnect;
}

namespace {

const base::DictionaryValue* GetNetworkConfigByGUID(
    const base::ListValue& network_configs,
    const std::string& guid) {
  for (base::ListValue::const_iterator it = network_configs.begin();
       it != network_configs.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    std::string current_guid;
    network->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
                                           &current_guid);
    if (current_guid == guid)
      return network;
  }
  return NULL;
}

const base::DictionaryValue* GetNetworkConfigForEthernetWithoutEAP(
    const base::ListValue& network_configs) {
  VLOG(2) << "Search for ethernet policy without EAP.";
  for (base::ListValue::const_iterator it = network_configs.begin();
       it != network_configs.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    DCHECK(network);

    std::string type;
    network->GetStringWithoutPathExpansion(::onc::network_config::kType, &type);
    if (type != ::onc::network_type::kEthernet)
      continue;

    const base::DictionaryValue* ethernet = NULL;
    network->GetDictionaryWithoutPathExpansion(::onc::network_config::kEthernet,
                                               &ethernet);

    std::string auth;
    ethernet->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                            &auth);
    if (auth == ::onc::ethernet::kAuthenticationNone)
      return network;
  }
  return NULL;
}

const base::DictionaryValue* GetNetworkConfigForNetworkFromOnc(
    const base::ListValue& network_configs,
    const NetworkState& network) {
  // In all cases except Ethernet, we use the GUID of |network|.
  if (!network.Matches(NetworkTypePattern::Ethernet()))
    return GetNetworkConfigByGUID(network_configs, network.guid());

  // Ethernet is always shared and thus cannot store a GUID per user. Thus we
  // search for any Ethernet policy intead of a matching GUID.
  // EthernetEAP service contains only the EAP parameters and stores the GUID of
  // the respective ONC policy. The EthernetEAP service itself is however never
  // in state "connected". An EthernetEAP policy must be applied, if an Ethernet
  // service is connected using the EAP parameters.
  const NetworkState* ethernet_eap = NULL;
  if (NetworkHandler::IsInitialized()) {
    ethernet_eap =
        NetworkHandler::Get()->network_state_handler()->GetEAPForEthernet(
            network.path());
  }

  // The GUID associated with the EthernetEAP service refers to the ONC policy
  // with "Authentication: 8021X".
  if (ethernet_eap)
    return GetNetworkConfigByGUID(network_configs, ethernet_eap->guid());

  // Otherwise, EAP is not used and instead the Ethernet policy with
  // "Authentication: None" applies.
  return GetNetworkConfigForEthernetWithoutEAP(network_configs);
}

const base::DictionaryValue* GetPolicyForNetworkFromPref(
    const PrefService* pref_service,
    const char* pref_name,
    const NetworkState& network) {
  if (!pref_service) {
    VLOG(2) << "No pref service";
    return NULL;
  }

  const PrefService::Preference* preference =
      pref_service->FindPreference(pref_name);
  if (!preference) {
    VLOG(2) << "No preference " << pref_name;
    // The preference may not exist in tests.
    return NULL;
  }

  // User prefs are not stored in this Preference yet but only the policy.
  //
  // The policy server incorrectly configures the OpenNetworkConfiguration user
  // policy as Recommended. To work around that, we handle the Recommended and
  // the Mandatory value in the same way.
  // TODO(pneubeck): Remove this workaround, once the server is fixed. See
  // http://crbug.com/280553 .
  if (preference->IsDefaultValue()) {
    VLOG(2) << "Preference has no recommended or mandatory value.";
    // No policy set.
    return NULL;
  }
  VLOG(2) << "Preference with policy found.";
  const base::Value* onc_policy_value = preference->GetValue();
  DCHECK(onc_policy_value);

  const base::ListValue* onc_policy = NULL;
  onc_policy_value->GetAsList(&onc_policy);
  DCHECK(onc_policy);

  return GetNetworkConfigForNetworkFromOnc(*onc_policy, network);
}

}  // namespace

const base::DictionaryValue* GetPolicyForNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const NetworkState& network,
    ::onc::ONCSource* onc_source) {
  VLOG(2) << "GetPolicyForNetwork: " << network.path();
  *onc_source = ::onc::ONC_SOURCE_NONE;

  const base::DictionaryValue* network_policy = GetPolicyForNetworkFromPref(
      profile_prefs, prefs::kOpenNetworkConfiguration, network);
  if (network_policy) {
    VLOG(1) << "Network " << network.path() << " is managed by user policy.";
    *onc_source = ::onc::ONC_SOURCE_USER_POLICY;
    return network_policy;
  }
  network_policy = GetPolicyForNetworkFromPref(
      local_state_prefs, prefs::kDeviceOpenNetworkConfiguration, network);
  if (network_policy) {
    VLOG(1) << "Network " << network.path() << " is managed by device policy.";
    *onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
    return network_policy;
  }
  VLOG(2) << "Network " << network.path() << " is unmanaged.";
  return NULL;
}

bool HasPolicyForNetwork(const PrefService* profile_prefs,
                         const PrefService* local_state_prefs,
                         const NetworkState& network) {
  ::onc::ONCSource ignored_onc_source;
  const base::DictionaryValue* policy = onc::GetPolicyForNetwork(
      profile_prefs, local_state_prefs, network, &ignored_onc_source);
  return policy != NULL;
}

}  // namespace onc
}  // namespace chromeos

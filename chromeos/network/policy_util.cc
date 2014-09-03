// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/policy_util.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace policy_util {

namespace {

// This fake credential contains a random postfix which is extremly unlikely to
// be used by any user.
const char kFakeCredential[] = "FAKE_CREDENTIAL_VPaJDV9x";


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
      if (field_signature)
        RemoveFakeCredentials(*field_signature->value_signature, nested_object);
      else
        LOG(ERROR) << "ONC has unrecoginzed field: " << field_name;
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

// Returns true if |policy| matches |actual_network|, which must be part of a
// ONC NetworkConfiguration. This should be the only such matching function
// within Chrome. Shill does such matching in several functions for network
// identification. For compatibility, we currently should stick to Shill's
// matching behavior.
bool IsPolicyMatching(const base::DictionaryValue& policy,
                      const base::DictionaryValue& actual_network) {
  std::string policy_type;
  policy.GetStringWithoutPathExpansion(::onc::network_config::kType,
                                       &policy_type);
  std::string actual_network_type;
  actual_network.GetStringWithoutPathExpansion(::onc::network_config::kType,
                                               &actual_network_type);
  if (policy_type != actual_network_type)
    return false;

  if (actual_network_type == ::onc::network_type::kEthernet) {
    const base::DictionaryValue* policy_ethernet = NULL;
    policy.GetDictionaryWithoutPathExpansion(::onc::network_config::kEthernet,
                                             &policy_ethernet);
    const base::DictionaryValue* actual_ethernet = NULL;
    actual_network.GetDictionaryWithoutPathExpansion(
        ::onc::network_config::kEthernet, &actual_ethernet);
    if (!policy_ethernet || !actual_ethernet)
      return false;

    std::string policy_auth;
    policy_ethernet->GetStringWithoutPathExpansion(
        ::onc::ethernet::kAuthentication, &policy_auth);
    std::string actual_auth;
    actual_ethernet->GetStringWithoutPathExpansion(
        ::onc::ethernet::kAuthentication, &actual_auth);
    return policy_auth == actual_auth;
  } else if (actual_network_type == ::onc::network_type::kWiFi) {
    const base::DictionaryValue* policy_wifi = NULL;
    policy.GetDictionaryWithoutPathExpansion(::onc::network_config::kWiFi,
                                             &policy_wifi);
    const base::DictionaryValue* actual_wifi = NULL;
    actual_network.GetDictionaryWithoutPathExpansion(
        ::onc::network_config::kWiFi,
        &actual_wifi);
    if (!policy_wifi || !actual_wifi)
      return false;

    std::string policy_ssid;
    policy_wifi->GetStringWithoutPathExpansion(::onc::wifi::kSSID,
                                               &policy_ssid);
    std::string actual_ssid;
    actual_wifi->GetStringWithoutPathExpansion(::onc::wifi::kSSID,
                                               &actual_ssid);
    return (policy_ssid == actual_ssid);
  }
  return false;
}

base::DictionaryValue* GetOrCreateDictionary(const std::string& key,
                                             base::DictionaryValue* dict) {
  base::DictionaryValue* inner_dict = NULL;
  if (!dict->GetDictionaryWithoutPathExpansion(key, &inner_dict)) {
    inner_dict = new base::DictionaryValue;
    dict->SetWithoutPathExpansion(key, inner_dict);
  }
  return inner_dict;
}

base::DictionaryValue* GetOrCreateNestedDictionary(
    const std::string& key1,
    const std::string& key2,
    base::DictionaryValue* dict) {
  base::DictionaryValue* inner_dict = GetOrCreateDictionary(key1, dict);
  return GetOrCreateDictionary(key2, inner_dict);
}

void ApplyGlobalAutoconnectPolicy(
    NetworkProfile::Type profile_type,
    base::DictionaryValue* augmented_onc_network) {
  base::DictionaryValue* type_dictionary = NULL;
  augmented_onc_network->GetDictionaryWithoutPathExpansion(
      ::onc::network_config::kType, &type_dictionary);
  std::string type;
  if (!type_dictionary ||
      !type_dictionary->GetStringWithoutPathExpansion(
          ::onc::kAugmentationActiveSetting, &type) ||
      type.empty()) {
    LOG(ERROR) << "ONC dictionary with no Type.";
    return;
  }

  // Managed dictionaries don't contain empty dictionaries (see onc_merger.cc),
  // so add the Autoconnect dictionary in case Shill didn't report a value.
  base::DictionaryValue* auto_connect_dictionary = NULL;
  if (type == ::onc::network_type::kWiFi) {
    auto_connect_dictionary =
        GetOrCreateNestedDictionary(::onc::network_config::kWiFi,
                                    ::onc::wifi::kAutoConnect,
                                    augmented_onc_network);
  } else if (type == ::onc::network_type::kVPN) {
    auto_connect_dictionary =
        GetOrCreateNestedDictionary(::onc::network_config::kVPN,
                                    ::onc::vpn::kAutoConnect,
                                    augmented_onc_network);
  } else {
    return;  // Network type without auto-connect property.
  }

  std::string policy_source;
  if (profile_type == NetworkProfile::TYPE_USER)
    policy_source = ::onc::kAugmentationUserPolicy;
  else if(profile_type == NetworkProfile::TYPE_SHARED)
    policy_source = ::onc::kAugmentationDevicePolicy;
  else
    NOTREACHED();

  auto_connect_dictionary->SetBooleanWithoutPathExpansion(policy_source, false);
  auto_connect_dictionary->SetStringWithoutPathExpansion(
      ::onc::kAugmentationEffectiveSetting, policy_source);
}

}  // namespace

scoped_ptr<base::DictionaryValue> CreateManagedONC(
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* active_settings,
    const NetworkProfile* profile) {
  const base::DictionaryValue* user_policy = NULL;
  const base::DictionaryValue* device_policy = NULL;
  const base::DictionaryValue* nonshared_user_settings = NULL;
  const base::DictionaryValue* shared_user_settings = NULL;

  if (profile) {
    if (profile->type() == NetworkProfile::TYPE_SHARED) {
      device_policy = network_policy;
      shared_user_settings = user_settings;
    } else if (profile->type() == NetworkProfile::TYPE_USER) {
      user_policy = network_policy;
      nonshared_user_settings = user_settings;
    } else {
      NOTREACHED();
    }
  }

  // This call also removes credentials from policies.
  scoped_ptr<base::DictionaryValue> augmented_onc_network =
      onc::MergeSettingsAndPoliciesToAugmented(
          onc::kNetworkConfigurationSignature,
          user_policy,
          device_policy,
          nonshared_user_settings,
          shared_user_settings,
          active_settings);

  // If present, apply the Autoconnect policy only to networks that are not
  // managed by policy.
  if (!network_policy && global_policy && profile) {
    bool allow_only_policy_autoconnect = false;
    global_policy->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
        &allow_only_policy_autoconnect);
    if (allow_only_policy_autoconnect) {
      ApplyGlobalAutoconnectPolicy(profile->type(),
                                   augmented_onc_network.get());
    }
  }

  return augmented_onc_network.Pass();
}

void SetShillPropertiesForGlobalPolicy(
    const base::DictionaryValue& shill_dictionary,
    const base::DictionaryValue& global_network_policy,
    base::DictionaryValue* shill_properties_to_update) {
  // kAllowOnlyPolicyNetworksToAutoconnect is currently the only global config.

  std::string type;
  shill_dictionary.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  if (NetworkTypePattern::Ethernet().MatchesType(type))
    return;  // Autoconnect for Ethernet cannot be configured.

  // By default all networks are allowed to autoconnect.
  bool only_policy_autoconnect = false;
  global_network_policy.GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  if (!only_policy_autoconnect)
    return;

  bool old_autoconnect = false;
  if (shill_dictionary.GetBooleanWithoutPathExpansion(
          shill::kAutoConnectProperty, &old_autoconnect) &&
      !old_autoconnect) {
    // Autoconnect is already explictly disabled. No need to set it again.
    return;
  }

  // If autconnect is not explicitly set yet, it might automatically be enabled
  // by Shill. To prevent that, disable it explicitly.
  shill_properties_to_update->SetBooleanWithoutPathExpansion(
      shill::kAutoConnectProperty, false);
}

scoped_ptr<base::DictionaryValue> CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings) {
  scoped_ptr<base::DictionaryValue> effective;
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  if (network_policy) {
    if (profile.type() == NetworkProfile::TYPE_SHARED) {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          NULL,  // no user policy
          network_policy,  // device policy
          NULL,  // no user settings
          user_settings);  // shared settings
      onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
    } else if (profile.type() == NetworkProfile::TYPE_USER) {
      effective = onc::MergeSettingsAndPoliciesToEffective(
          network_policy,  // user policy
          NULL,  // no device policy
          user_settings,  // user settings
          NULL);  // no shared settings
      onc_source = ::onc::ONC_SOURCE_USER_POLICY;
    } else {
      NOTREACHED();
    }
  } else if (user_settings) {
    effective.reset(user_settings->DeepCopy());
    // TODO(pneubeck): change to source ONC_SOURCE_USER
    onc_source = ::onc::ONC_SOURCE_NONE;
  } else {
    NOTREACHED();
    onc_source = ::onc::ONC_SOURCE_NONE;
  }

  RemoveFakeCredentials(onc::kNetworkConfigurationSignature,
                        effective.get());

  effective->SetStringWithoutPathExpansion(::onc::network_config::kGUID, guid);

  // Remove irrelevant fields.
  onc::Normalizer normalizer(true /* remove recommended fields */);
  effective = normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                         *effective);

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                     *effective));

  shill_dictionary->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                                  profile.path);

  if (!network_policy && global_policy) {
    // The network isn't managed. Global network policies have to be applied.
    SetShillPropertiesForGlobalPolicy(
        *shill_dictionary, *global_policy, shill_dictionary.get());
  }

  scoped_ptr<NetworkUIData> ui_data(NetworkUIData::CreateFromONC(onc_source));

  if (user_settings) {
    // Shill doesn't know that sensitive data is contained in the UIData
    // property and might write it into logs or other insecure places. Thus, we
    // have to remove or mask credentials.
    //
    // Shill's GetProperties doesn't return credentials. Masking credentials
    // instead of just removing them, allows remembering if a credential is set
    // or not.
    scoped_ptr<base::DictionaryValue> sanitized_user_settings(
        onc::MaskCredentialsInOncObject(onc::kNetworkConfigurationSignature,
                                        *user_settings,
                                        kFakeCredential));
    ui_data->set_user_settings(sanitized_user_settings.Pass());
  }

  shill_property_util::SetUIData(*ui_data, shill_dictionary.get());

  VLOG(2) << "Created Shill properties: " << *shill_dictionary;

  return shill_dictionary.Pass();
}

const base::DictionaryValue* FindMatchingPolicy(
    const GuidToPolicyMap& policies,
    const base::DictionaryValue& actual_network) {
  for (GuidToPolicyMap::const_iterator it = policies.begin();
       it != policies.end(); ++it) {
    if (IsPolicyMatching(*it->second, actual_network))
      return it->second;
  }
  return NULL;
}

}  // namespace policy_util

}  // namespace chromeos

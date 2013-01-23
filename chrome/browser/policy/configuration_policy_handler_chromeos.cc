// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_chromeos.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace onc = chromeos::onc;

namespace {

}  // namespace

namespace policy {

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    const char* policy_name,
    chromeos::onc::ONCSource onc_source)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_STRING),
      onc_source_(onc_source) {}

NetworkConfigurationPolicyHandler::~NetworkConfigurationPolicyHandler() {}

bool NetworkConfigurationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (value) {
    std::string onc_blob;
    value->GetAsString(&onc_blob);
    scoped_ptr<base::DictionaryValue> root_dict =
        onc::ReadDictionaryFromJson(onc_blob);
    if (root_dict.get() == NULL) {
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_PARSE_FAILED);
      return false;
    }

    // Validate the ONC dictionary. We are liberal and ignore unknown field
    // names and ignore invalid field names in kRecommended arrays.
    onc::Validator validator(false,  // Ignore unknown fields.
                             false,  // Ignore invalid recommended field names.
                             true,  // Fail on missing fields.
                             true);  // Validate for managed ONC
    validator.SetOncSource(onc_source_);

    // ONC policies are always unencrypted.
    onc::Validator::Result validation_result;
    root_dict = validator.ValidateAndRepairObject(
        &onc::kToplevelConfigurationSignature, *root_dict, &validation_result);
    if (validation_result == onc::Validator::VALID_WITH_WARNINGS) {
      errors->AddError(policy_name(),
                       IDS_POLICY_NETWORK_CONFIG_IMPORT_PARTIAL);
    } else if (validation_result == onc::Validator::INVALID) {
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_FAILED);
    }

    // In any case, don't reject the policy as some networks or certificates
    // could still be applied.
  }

  return true;
}

void NetworkConfigurationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // Network policy is read directly from the provider and injected into
  // NetworkLibrary, so no need to convert the policy settings into prefs.
}

void NetworkConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  const PolicyMap::Entry* entry = policies->Get(policy_name());
  if (!entry)
    return;
  base::Value* sanitized_config = SanitizeNetworkConfig(entry->value);
  if (!sanitized_config)
    sanitized_config = base::Value::CreateNullValue();

  policies->Set(policy_name(), entry->level, entry->scope, sanitized_config);
}

// static
base::Value* NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const base::Value* config) {
  std::string json_string;
  if (!config->GetAsString(&json_string))
    return NULL;

  scoped_ptr<base::Value> json_value(
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!json_value.get() || !json_value->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;

  base::DictionaryValue* config_dict =
      static_cast<base::DictionaryValue*>(json_value.get());

  // Strip any sensitive information from the JSON dictionary.
  base::ListValue* config_list = NULL;
  if (config_dict->GetList("NetworkConfigurations", &config_list)) {
    for (base::ListValue::const_iterator network_entry = config_list->begin();
         network_entry != config_list->end();
         ++network_entry) {
      if ((*network_entry) &&
          (*network_entry)->IsType(base::Value::TYPE_DICTIONARY)) {
        MaskSensitiveValues(
            static_cast<base::DictionaryValue*>(*network_entry));
      }
    }
  }

  // Convert back to a string, pretty printing the contents.
  base::JSONWriter::WriteWithOptions(config_dict,
                                     base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_string);
  return base::Value::CreateStringValue(json_string);
}

// static
void NetworkConfigurationPolicyHandler::MaskSensitiveValues(
    base::DictionaryValue* network_dict) {
  // Paths of the properties to be replaced by the placeholder. Each entry
  // specifies dictionary key paths.
  static const int kMaxComponents = 3;
  static const char* kFilteredSettings[][kMaxComponents] = {
    { onc::network_config::kEthernet, onc::ethernet::kEAP,
      onc::eap::kPassword },
    { onc::network_config::kVPN, onc::vpn::kIPsec, onc::vpn::kPSK },
    { onc::network_config::kVPN, onc::vpn::kL2TP, onc::vpn::kPassword },
    { onc::network_config::kVPN, onc::vpn::kOpenVPN, onc::vpn::kPassword },
    { onc::network_config::kVPN, onc::vpn::kOpenVPN,
      onc::vpn::kTLSAuthContents },
    { onc::network_config::kWiFi, onc::wifi::kEAP, onc::eap::kPassword },
    { onc::network_config::kWiFi, onc::wifi::kPassphrase },
  };

  // Placeholder to insert in place of the filtered setting.
  static const char kPlaceholder[] = "********";

  for (size_t i = 0; i < arraysize(kFilteredSettings); ++i) {
    const char** path = kFilteredSettings[i];
    base::DictionaryValue* dict = network_dict;
    int j = 0;
    for (j = 0; path[j + 1] != NULL && j + 1 < kMaxComponents; ++j) {
      if (!dict->GetDictionaryWithoutPathExpansion(path[j], &dict)) {
        dict = NULL;
        break;
      }
    }
    if (dict && dict->RemoveWithoutPathExpansion(path[j], NULL)) {
      dict->SetWithoutPathExpansion(
          path[j], base::Value::CreateStringValue(kPlaceholder));
    }
  }
}

PinnedLauncherAppsPolicyHandler::PinnedLauncherAppsPolicyHandler()
    : ExtensionListPolicyHandler(key::kPinnedLauncherApps,
                                 prefs::kPinnedLauncherApps,
                                 false) {}

PinnedLauncherAppsPolicyHandler::~PinnedLauncherAppsPolicyHandler() {}

void PinnedLauncherAppsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  PolicyErrorMap errors;
  const base::Value* policy_value = policies.GetValue(policy_name());
  const base::ListValue* policy_list = NULL;
  if (policy_value && policy_value->GetAsList(&policy_list) && policy_list) {
    base::ListValue* pinned_apps_list = new base::ListValue();
    for (base::ListValue::const_iterator entry(policy_list->begin());
         entry != policy_list->end(); ++entry) {
      std::string id;
      if ((*entry)->GetAsString(&id)) {
        base::DictionaryValue* app_dict = new base::DictionaryValue();
        app_dict->SetString(ash::kPinnedAppsPrefAppIDPath, id);
        pinned_apps_list->Append(app_dict);
      }
    }
    prefs->SetValue(pref_path(), pinned_apps_list);
  }
}

}  // namespace policy

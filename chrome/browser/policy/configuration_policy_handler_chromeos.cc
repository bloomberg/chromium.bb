// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_chromeos.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/onc_network_parser.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    const char* policy_name,
    chromeos::NetworkUIData::ONCSource onc_source)
    : TypeCheckingPolicyHandler(policy_name, Value::TYPE_STRING),
      onc_source_(onc_source) {}

NetworkConfigurationPolicyHandler::~NetworkConfigurationPolicyHandler() {}

bool NetworkConfigurationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const Value* value;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (value) {
    std::string onc_blob;
    value->GetAsString(&onc_blob);
    // Policy-based ONC blobs cannot have a passphrase.
    chromeos::OncNetworkParser parser(onc_blob, "", onc_source_);
    if (!parser.parse_error().empty()) {
      errors->AddError(policy_name(),
                       IDS_POLICY_NETWORK_CONFIG_PARSE_ERROR,
                       parser.parse_error());
      return false;
    }
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
  Value* sanitized_config = SanitizeNetworkConfig(entry->value);
  if (!sanitized_config)
    sanitized_config = Value::CreateNullValue();

  policies->Set(policy_name(), entry->level, entry->scope, sanitized_config);
}

// static
Value* NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const Value* config) {
  std::string json_string;
  if (!config->GetAsString(&json_string))
    return NULL;

  scoped_ptr<Value> json_value(
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!json_value.get() || !json_value->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;

  DictionaryValue* config_dict =
      static_cast<DictionaryValue*>(json_value.get());

  // Strip any sensitive information from the JSON dictionary.
  base::ListValue* config_list = NULL;
  if (config_dict->GetList("NetworkConfigurations", &config_list)) {
    for (base::ListValue::const_iterator network_entry = config_list->begin();
         network_entry != config_list->end();
         ++network_entry) {
      if ((*network_entry) &&
          (*network_entry)->IsType(base::Value::TYPE_DICTIONARY)) {
        StripSensitiveValues(static_cast<DictionaryValue*>(*network_entry));
      }
    }
  }

  // Convert back to a string, pretty printing the contents.
  base::JSONWriter::WriteWithOptions(config_dict,
                                     base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_string);
  return Value::CreateStringValue(json_string);
}

// static
void NetworkConfigurationPolicyHandler::StripSensitiveValues(
    DictionaryValue* network_dict) {
  // List of settings we filter from the network dictionary.
  static const char* kFilteredSettings[] = {
    "WiFi.Passphrase",
    "IPsec.EAP.Password",
    "IPsec.EAP.Password",
    "IPsec.XAUTH.Password",
    "L2TP.Password",
  };
  // Placeholder to insert in place of the filtered setting.
  static const char kPlaceholder[] = "********";

  for (size_t i = 0; i < arraysize(kFilteredSettings); ++i) {
    if (network_dict->Remove(kFilteredSettings[i], NULL)) {
      network_dict->Set(kFilteredSettings[i],
                        Value::CreateStringValue(kPlaceholder));
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
        app_dict->SetString(ChromeLauncherController::kPinnedAppsPrefAppIDPath,
                            id);
        pinned_apps_list->Append(app_dict);
      }
    }
    prefs->SetValue(pref_path(), pinned_apps_list);
  }
}

}  // namespace policy

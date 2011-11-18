// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "grit/generated_resources.h"

namespace policy {

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    ConfigurationPolicyType type)
    : TypeCheckingPolicyHandler(type, Value::TYPE_STRING) {}

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
    chromeos::OncNetworkParser parser(onc_blob);
    if (!parser.parse_error().empty()) {
      errors->AddError(policy_type(),
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
  const Value* network_config = policies->Get(policy_type());
  if (!network_config)
    return;

  Value* sanitized_config = SanitizeNetworkConfig(network_config);
  if (!sanitized_config)
    sanitized_config = Value::CreateNullValue();

  policies->Set(policy_type(), sanitized_config);
}

// static
Value* NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const Value* config) {
  std::string json_string;
  if (!config->GetAsString(&json_string))
    return NULL;

  scoped_ptr<Value> json_value(base::JSONReader::Read(json_string, true));
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
  base::JSONWriter::WriteWithOptions(config_dict, true,
                                     base::JSONWriter::OPTIONS_DO_NOT_ESCAPE,
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

}  // namespace policy

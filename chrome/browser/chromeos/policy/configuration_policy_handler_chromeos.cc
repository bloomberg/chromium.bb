// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include <string>

#include "ash/magnifier/magnifier_constants.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/login_screen_power_management_policy.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "components/onc/onc_constants.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForUserPolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kOpenNetworkConfiguration,
      onc::ONC_SOURCE_USER_POLICY,
      prefs::kOpenNetworkConfiguration);
}

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForDevicePolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kDeviceOpenNetworkConfiguration,
      onc::ONC_SOURCE_DEVICE_POLICY,
      prefs::kDeviceOpenNetworkConfiguration);
}

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
        chromeos::onc::ReadDictionaryFromJson(onc_blob);
    if (root_dict.get() == NULL) {
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_PARSE_FAILED);
      return false;
    }

    // Validate the ONC dictionary. We are liberal and ignore unknown field
    // names and ignore invalid field names in kRecommended arrays.
    chromeos::onc::Validator validator(
        false,  // Ignore unknown fields.
        false,  // Ignore invalid recommended field names.
        true,   // Fail on missing fields.
        true);  // Validate for managed ONC
    validator.SetOncSource(onc_source_);

    // ONC policies are always unencrypted.
    chromeos::onc::Validator::Result validation_result;
    root_dict = validator.ValidateAndRepairObject(
        &chromeos::onc::kToplevelConfigurationSignature, *root_dict,
        &validation_result);
    if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS)
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_PARTIAL);
    else if (validation_result == chromeos::onc::Validator::INVALID)
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_FAILED);

    // In any case, don't reject the policy as some networks or certificates
    // could still be applied.
  }

  return true;
}

void NetworkConfigurationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  std::string onc_blob;
  value->GetAsString(&onc_blob);

  scoped_ptr<base::ListValue> network_configs(new base::ListValue);
  base::ListValue certificates;
  base::DictionaryValue global_network_config;
  chromeos::onc::ParseAndValidateOncForImport(onc_blob,
                                              onc_source_,
                                              "",
                                              network_configs.get(),
                                              &global_network_config,
                                              &certificates);

  // Currently, only the per-network configuration is stored in a pref. Ignore
  // |global_network_config| and |certificates|.
  prefs->SetValue(pref_path_, network_configs.release());
}

void NetworkConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  const PolicyMap::Entry* entry = policies->Get(policy_name());
  if (!entry)
    return;
  base::Value* sanitized_config = SanitizeNetworkConfig(entry->value);
  if (!sanitized_config)
    sanitized_config = base::Value::CreateNullValue();

  policies->Set(policy_name(), entry->level, entry->scope,
                sanitized_config, NULL);
}

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    const char* policy_name,
    onc::ONCSource onc_source,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_STRING),
      onc_source_(onc_source),
      pref_path_(pref_path) {
}

// static
base::Value* NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const base::Value* config) {
  std::string json_string;
  if (!config->GetAsString(&json_string))
    return NULL;

  scoped_ptr<base::DictionaryValue> toplevel_dict =
      chromeos::onc::ReadDictionaryFromJson(json_string);
  if (!toplevel_dict)
    return NULL;

  // Placeholder to insert in place of the filtered setting.
  const char kPlaceholder[] = "********";

  toplevel_dict = chromeos::onc::MaskCredentialsInOncObject(
      chromeos::onc::kToplevelConfigurationSignature,
      *toplevel_dict,
      kPlaceholder);

  base::JSONWriter::WriteWithOptions(toplevel_dict.get(),
                                     base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_string);
  return base::Value::CreateStringValue(json_string);
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

ScreenMagnifierPolicyHandler::ScreenMagnifierPolicyHandler()
    : IntRangePolicyHandlerBase(key::kScreenMagnifierType,
                                0, ash::MAGNIFIER_FULL, false) {
}

ScreenMagnifierPolicyHandler::~ScreenMagnifierPolicyHandler() {
}

void ScreenMagnifierPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, NULL)) {
    prefs->SetValue(prefs::kScreenMagnifierEnabled,
                    base::Value::CreateBooleanValue(value_in_range != 0));
    prefs->SetValue(prefs::kScreenMagnifierType,
                    base::Value::CreateIntegerValue(value_in_range));
  }
}

LoginScreenPowerManagementPolicyHandler::
    LoginScreenPowerManagementPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDeviceLoginScreenPowerManagement,
                                base::Value::TYPE_STRING) {
}

LoginScreenPowerManagementPolicyHandler::
    ~LoginScreenPowerManagementPolicyHandler() {
}

bool LoginScreenPowerManagementPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  std::string json;
  value->GetAsString(&json);
  return LoginScreenPowerManagementPolicy().Init(json, errors);
}

void LoginScreenPowerManagementPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
}

DeprecatedIdleActionHandler::DeprecatedIdleActionHandler()
    : IntRangePolicyHandlerBase(
          key::kIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false) {}

DeprecatedIdleActionHandler::~DeprecatedIdleActionHandler() {}

void DeprecatedIdleActionHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (value && EnsureInRange(value, NULL, NULL)) {
    if (!prefs->GetValue(prefs::kPowerAcIdleAction, NULL))
      prefs->SetValue(prefs::kPowerAcIdleAction, value->DeepCopy());
    if (!prefs->GetValue(prefs::kPowerBatteryIdleAction, NULL))
      prefs->SetValue(prefs::kPowerBatteryIdleAction, value->DeepCopy());
  }
}

}  // namespace policy

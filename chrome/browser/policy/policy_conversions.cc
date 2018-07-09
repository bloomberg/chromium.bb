// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_conversions.h"

#include <unordered_set>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#endif

namespace em = enterprise_management;

namespace policy {

namespace {

struct PolicyStringMap {
  const char* key;
  int string_id;
};

const PolicyStringMap kPolicySources[policy::POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"sourceCloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourcePublicSessionOverride", IDS_POLICY_SOURCE_PUBLIC_SESSION_OVERRIDE},
    {"sourcePlatform", IDS_POLICY_SOURCE_PLATFORM},
};

// Utility function that returns a JSON serialization of the given |dict|.
std::unique_ptr<base::Value> DictionaryToJSONString(
    const base::DictionaryValue& dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
  return std::make_unique<base::Value>(json_string);
}

// Returns a copy of |value|. If necessary (which is specified by
// |convert_values|), converts some values to a representation that
// i18n_template.js will display.
std::unique_ptr<base::Value> CopyAndMaybeConvert(const base::Value* value,
                                                 bool convert_values) {
  if (!convert_values)
    return value->CreateDeepCopy();
  const base::DictionaryValue* dict = NULL;
  if (value->GetAsDictionary(&dict))
    return DictionaryToJSONString(*dict);

  std::unique_ptr<base::Value> copy = value->CreateDeepCopy();
  base::ListValue* list = NULL;
  if (copy->GetAsList(&list)) {
    for (size_t i = 0; i < list->GetSize(); ++i) {
      if (list->GetDictionary(i, &dict))
        list->Set(i, DictionaryToJSONString(*dict));
    }
  }

  return copy;
}

PolicyService* GetPolicyService(content::BrowserContext* context) {
  return ProfilePolicyConnectorFactory::GetForBrowserContext(context)
      ->policy_service();
}

// Inserts a description of each policy in |policy_map| into |values|, using
// the optional errors in |errors| to determine the status of each policy. If
// |convert_values| is true, converts the values to show them in javascript.
// |policy_names| contains all valid policies in the same policy namespace of
// |policy_map|. A policy in |map| but not|policy_names| is an unknown policy.
void GetPolicyValues(
    const policy::PolicyMap& map,
    policy::PolicyErrorMap* errors,
    base::DictionaryValue* values,
    bool with_user_policies,
    bool convert_values,
    std::unique_ptr<std::unordered_set<std::string>> policy_names) {
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == policy::POLICY_SCOPE_USER && !with_user_policies)
      continue;

    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    value->Set("value",
               CopyAndMaybeConvert(policy.value.get(), convert_values));
    value->SetString("scope", (policy.scope == policy::POLICY_SCOPE_USER)
                                  ? "user"
                                  : "machine");
    value->SetString("level", (policy.level == policy::POLICY_LEVEL_RECOMMENDED)
                                  ? "recommended"
                                  : "mandatory");
    value->SetString("source", kPolicySources[policy.source].key);
    base::string16 error;
    if (policy_names &&
        policy_names->find(policy_name) == policy_names->end()) {
      // We don't know what this policy is. This is an important error to show.
      error = l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
    } else {
      // The PolicyMap contains errors about retrieving the policy, while the
      // PolicyErrorMap contains validation errors. Give priority to PolicyMap.
      error = !policy.error.empty() ? base::UTF8ToUTF16(policy.error)
                                    : errors->GetErrors(policy_name);
    }
    if (!error.empty())
      value->SetString("error", error);
    values->SetWithoutPathExpansion(policy_name, std::move(value));
  }
}

std::unique_ptr<std::unordered_set<std::string>> GetPolicyNameSet(
    const scoped_refptr<policy::SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace) {
  const Schema* schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid())
    return nullptr;

  auto policy_names = std::make_unique<std::unordered_set<std::string>>();

  for (policy::Schema::Iterator it = schema->GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    policy_names->insert(it.key());
  }
  return policy_names;
}

void GetChromePolicyValues(content::BrowserContext* context,
                           base::DictionaryValue* values,
                           bool keep_user_policies,
                           bool convert_values) {
  policy::PolicyService* policy_service = GetPolicyService(context);
  policy::PolicyMap map;
  const scoped_refptr<policy::SchemaMap> schema_map =
      SchemaRegistryServiceFactory::GetForContext(context)
          ->registry()
          ->schema_map();
  PolicyNamespace policy_namespace =
      PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  map.CopyFrom(policy_service->GetPolicies(policy_namespace));

  // Get a list of all the errors in the policy values.
  const policy::ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  policy::PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(map, NULL, &errors);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  GetPolicyValues(map, &errors, values, keep_user_policies, convert_values,
                  GetPolicyNameSet(schema_map, policy_namespace));
}

}  // namespace

std::unique_ptr<base::DictionaryValue> GetAllPolicyValuesAsDictionary(
    content::BrowserContext* context,
    bool with_user_policies,
    bool convert_values) {
  base::DictionaryValue all_policies;
  if (!context)
    return std::make_unique<base::DictionaryValue>(std::move(all_policies));

  context = chrome::GetBrowserContextRedirectedInIncognito(context);

  // Add Chrome policy values.
  auto chrome_policies = std::make_unique<base::DictionaryValue>();
  GetChromePolicyValues(context, chrome_policies.get(), with_user_policies,
                        convert_values);
  all_policies.Set("chromePolicies", std::move(chrome_policies));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy values.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromBrowserContext(context));
  auto extension_values = std::make_unique<base::DictionaryValue>();
  const scoped_refptr<policy::SchemaMap> schema_map =
      SchemaRegistryServiceFactory::GetForContext(context)
          ->registry()
          ->schema_map();
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema))
      continue;
    auto extension_policies = std::make_unique<base::DictionaryValue>();
    policy::PolicyNamespace policy_namespace = policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, extension->id());
    policy::PolicyErrorMap empty_error_map;
    GetPolicyValues(GetPolicyService(context)->GetPolicies(policy_namespace),
                    &empty_error_map, extension_policies.get(),
                    with_user_policies, convert_values,
                    GetPolicyNameSet(schema_map, policy_namespace));
    extension_values->Set(extension->id(), std::move(extension_policies));
  }
  all_policies.Set("extensionPolicies", std::move(extension_values));
#endif
  return std::make_unique<base::DictionaryValue>(std::move(all_policies));
}

#if defined(OS_CHROMEOS)
void FillIdentityFieldsFromPolicy(base::DictionaryValue* policy_dump,
                                  const em::PolicyData* policy) {
  if (!policy) {
    return;
  }

  if (policy->has_device_id())
    policy_dump->SetString("client_id", policy->device_id());

  if (policy->has_annotated_location())
    policy_dump->SetString("device_location", policy->annotated_location());

  if (policy->has_annotated_asset_id())
    policy_dump->SetString("asset_id", policy->annotated_asset_id());

  if (policy->has_display_domain())
    policy_dump->SetString("display_domain", policy->display_domain());

  if (policy->has_machine_name())
    policy_dump->SetString("machine_name", policy->machine_name());
}
#endif  // defined(OS_CHROMEOS)

void FillIdentityFields(base::DictionaryValue* policy_dump) {
#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    policy_dump->SetString("enrollment_domain",
                           connector->GetEnterpriseEnrollmentDomain());

    if (connector->IsActiveDirectoryManaged()) {
      FillIdentityFieldsFromPolicy(
          policy_dump, connector->GetDeviceActiveDirectoryPolicyManager()
                           ->store()
                           ->policy());
    }

    if (connector->IsCloudManaged()) {
      FillIdentityFieldsFromPolicy(
          policy_dump,
          connector->GetDeviceCloudPolicyManager()->device_store()->policy());
    }
  }
#endif  // defined(OS_CHROMEOS)
}

std::string GetAllPolicyValuesAsJSON(content::BrowserContext* context,
                                     bool with_user_policies,
                                     bool with_device_identity) {
  std::unique_ptr<base::DictionaryValue> all_policies =
      policy::GetAllPolicyValuesAsDictionary(context, with_user_policies,
                                             false /* convert_values */);
  if (with_device_identity) {
    FillIdentityFields(all_policies.get());
  }
  return DictionaryToJSONString(*all_policies)->GetString();
}

}  // namespace policy

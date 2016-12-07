// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_handlers.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

// ExtensionListPolicyHandler implementation -----------------------------------

ExtensionListPolicyHandler::ExtensionListPolicyHandler(const char* policy_name,
                                                       const char* pref_path,
                                                       bool allow_wildcards)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

ExtensionListPolicyHandler::~ExtensionListPolicyHandler() {}

bool ExtensionListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return CheckAndGetList(policies, errors, NULL);
}

void ExtensionListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::ListValue> list;
  policy::PolicyErrorMap errors;
  if (CheckAndGetList(policies, &errors, &list) && list)
    prefs->SetValue(pref_path(), std::move(list));
}

const char* ExtensionListPolicyHandler::pref_path() const {
  return pref_path_;
}

bool ExtensionListPolicyHandler::CheckAndGetList(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors,
    std::unique_ptr<base::ListValue>* extension_ids) {
  if (extension_ids)
    extension_ids->reset();

  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  // Filter the list, rejecting any invalid extension IDs.
  std::unique_ptr<base::ListValue> filtered_list(new base::ListValue());
  for (base::ListValue::const_iterator entry(list_value->begin());
       entry != list_value->end(); ++entry) {
    std::string id;
    if (!(*entry)->GetAsString(&id)) {
      errors->AddError(policy_name(), entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING));
      continue;
    }
    if (!(allow_wildcards_ && id == "*") && !crx_file::id_util::IdIsValid(id)) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_VALUE_FORMAT_ERROR);
      continue;
    }
    filtered_list->AppendString(id);
  }

  if (extension_ids)
    *extension_ids = std::move(filtered_list);

  return true;
}

// ExtensionInstallForcelistPolicyHandler implementation -----------------------

ExtensionInstallForcelistPolicyHandler::ExtensionInstallForcelistPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kExtensionInstallForcelist,
                                        base::Value::Type::LIST) {}

ExtensionInstallForcelistPolicyHandler::
    ~ExtensionInstallForcelistPolicyHandler() {}

bool ExtensionInstallForcelistPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
      ParseList(value, NULL, errors);
}

void ExtensionInstallForcelistPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = NULL;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  if (CheckAndGetValue(policies, NULL, &value) &&
      value &&
      ParseList(value, dict.get(), NULL)) {
    prefs->SetValue(pref_names::kInstallForceList, std::move(dict));
  }
}

bool ExtensionInstallForcelistPolicyHandler::ParseList(
    const base::Value* policy_value,
    base::DictionaryValue* extension_dict,
    policy::PolicyErrorMap* errors) {
  if (!policy_value)
    return true;

  const base::ListValue* policy_list_value = NULL;
  if (!policy_value->GetAsList(&policy_list_value)) {
    // This should have been caught in CheckPolicySettings.
    NOTREACHED();
    return false;
  }

  for (base::ListValue::const_iterator entry(policy_list_value->begin());
       entry != policy_list_value->end(); ++entry) {
    std::string entry_string;
    if (!(*entry)->GetAsString(&entry_string)) {
      if (errors) {
        errors->AddError(policy_name(), entry - policy_list_value->begin(),
                         IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING));
      }
      continue;
    }

    // Each string item of the list has the following form:
    // <extension_id>;<update_url>
    // Note: The update URL might also contain semicolons.
    size_t pos = entry_string.find(';');
    if (pos == std::string::npos) {
      if (errors) {
        errors->AddError(policy_name(),
                         entry - policy_list_value->begin(),
                         IDS_POLICY_VALUE_FORMAT_ERROR);
      }
      continue;
    }

    std::string extension_id = entry_string.substr(0, pos);
    std::string update_url = entry_string.substr(pos+1);
    if (!crx_file::id_util::IdIsValid(extension_id) ||
        !GURL(update_url).is_valid()) {
      if (errors) {
        errors->AddError(policy_name(),
                         entry - policy_list_value->begin(),
                         IDS_POLICY_VALUE_FORMAT_ERROR);
      }
      continue;
    }

    if (extension_dict) {
      extensions::ExternalPolicyLoader::AddExtension(
          extension_dict, extension_id, update_url);
    }
  }

  return true;
}

// ExtensionURLPatternListPolicyHandler implementation -------------------------

ExtensionURLPatternListPolicyHandler::ExtensionURLPatternListPolicyHandler(
    const char* policy_name,
    const char* pref_path)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

ExtensionURLPatternListPolicyHandler::~ExtensionURLPatternListPolicyHandler() {}

bool ExtensionURLPatternListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  // Check that the list contains valid URLPattern strings only.
  for (base::ListValue::const_iterator entry(list_value->begin());
       entry != list_value->end(); ++entry) {
    std::string url_pattern_string;
    if (!(*entry)->GetAsString(&url_pattern_string)) {
      errors->AddError(policy_name(), entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING));
      return false;
    }

    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(url_pattern_string) != URLPattern::PARSE_SUCCESS) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_VALUE_FORMAT_ERROR);
      return false;
    }
  }

  return true;
}

void ExtensionURLPatternListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->CreateDeepCopy());
}

// ExtensionSettingsPolicyHandler implementation  ------------------------------

ExtensionSettingsPolicyHandler::ExtensionSettingsPolicyHandler(
    const policy::Schema& chrome_schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kExtensionSettings,
          chrome_schema.GetKnownProperty(policy::key::kExtensionSettings),
          policy::SCHEMA_ALLOW_UNKNOWN) {
}

ExtensionSettingsPolicyHandler::~ExtensionSettingsPolicyHandler() {
}

bool ExtensionSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, errors, &policy_value))
    return false;
  if (!policy_value)
    return true;

  // |policy_value| is expected to conform to the defined schema. But it's
  // not strictly valid since there are additional restrictions.
  const base::DictionaryValue* dict_value = NULL;
  DCHECK(policy_value->IsType(base::Value::Type::DICTIONARY));
  policy_value->GetAsDictionary(&dict_value);

  for (base::DictionaryValue::Iterator it(*dict_value); !it.IsAtEnd();
       it.Advance()) {
    DCHECK(it.key() == schema_constants::kWildcard ||
           crx_file::id_util::IdIsValid(it.key()));
    DCHECK(it.value().IsType(base::Value::Type::DICTIONARY));

    // Extracts sub dictionary.
    const base::DictionaryValue* sub_dict = NULL;
    it.value().GetAsDictionary(&sub_dict);

    std::string installation_mode;
    if (sub_dict->GetString(schema_constants::kInstallationMode,
                            &installation_mode)) {
      if (installation_mode == schema_constants::kForceInstalled ||
          installation_mode == schema_constants::kNormalInstalled) {
        DCHECK(it.key() != schema_constants::kWildcard);
        // Verifies that 'update_url' is specified for 'force_installed' and
        // 'normal_installed' mode.
        std::string update_url;
        if (!sub_dict->GetString(schema_constants::kUpdateUrl, &update_url) ||
            update_url.empty()) {
          errors->AddError(policy_name(),
                           it.key() + "." + schema_constants::kUpdateUrl,
                           IDS_POLICY_NOT_SPECIFIED_ERROR);
          return false;
        }
        // Verifies that update URL is valid.
        if (!GURL(update_url).is_valid()) {
          errors->AddError(
              policy_name(), IDS_POLICY_INVALID_UPDATE_URL_ERROR, it.key());
          return false;
        }
      }
    }
  }

  return true;
}

void ExtensionSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, NULL, &policy_value) || !policy_value)
    return;
  prefs->SetValue(pref_names::kExtensionManagement, std::move(policy_value));
}

}  // namespace extensions

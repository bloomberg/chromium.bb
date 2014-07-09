// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_handlers.h"

#include "base/logging.h"
#include "base/prefs/pref_value_map.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

namespace extensions {

// ExtensionListPolicyHandler implementation -----------------------------------

ExtensionListPolicyHandler::ExtensionListPolicyHandler(const char* policy_name,
                                                       const char* pref_path,
                                                       bool allow_wildcards)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
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
  scoped_ptr<base::ListValue> list;
  policy::PolicyErrorMap errors;
  if (CheckAndGetList(policies, &errors, &list) && list)
    prefs->SetValue(pref_path(), list.release());
}

const char* ExtensionListPolicyHandler::pref_path() const {
  return pref_path_;
}

bool ExtensionListPolicyHandler::CheckAndGetList(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors,
    scoped_ptr<base::ListValue>* extension_ids) {
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
  scoped_ptr<base::ListValue> filtered_list(new base::ListValue());
  for (base::ListValue::const_iterator entry(list_value->begin());
       entry != list_value->end(); ++entry) {
    std::string id;
    if (!(*entry)->GetAsString(&id)) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(base::Value::TYPE_STRING));
      continue;
    }
    if (!(allow_wildcards_ && id == "*") &&
        !extensions::Extension::IdIsValid(id)) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_VALUE_FORMAT_ERROR);
      continue;
    }
    filtered_list->Append(new base::StringValue(id));
  }

  if (extension_ids)
    *extension_ids = filtered_list.Pass();

  return true;
}

// ExtensionInstallForcelistPolicyHandler implementation -----------------------

ExtensionInstallForcelistPolicyHandler::ExtensionInstallForcelistPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kExtensionInstallForcelist,
                                        base::Value::TYPE_LIST) {}

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
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  if (CheckAndGetValue(policies, NULL, &value) &&
      value &&
      ParseList(value, dict.get(), NULL)) {
    prefs->SetValue(pref_names::kInstallForceList, dict.release());
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
        errors->AddError(policy_name(),
                         entry - policy_list_value->begin(),
                         IDS_POLICY_TYPE_ERROR,
                         ValueTypeToString(base::Value::TYPE_STRING));
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
    if (!extensions::Extension::IdIsValid(extension_id) ||
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
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
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
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(base::Value::TYPE_STRING));
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
    prefs->SetValue(pref_path_, value->DeepCopy());
}

}  // namespace extensions

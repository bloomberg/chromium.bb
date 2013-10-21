// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"

#include <algorithm>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/prefs/pref_value_map.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/policy/policy_path_parser.h"
#endif

namespace policy {

namespace {

// Helper function -------------------------------------------------------------

// Utility function that returns a JSON representation of the given |dict| as
// a StringValue. The caller owns the returned object.
base::StringValue* DictionaryToJSONString(const base::DictionaryValue* dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      dict,
      base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
      &json_string);
  return Value::CreateStringValue(json_string);
}


}  // namespace


// ConfigurationPolicyHandler implementation -----------------------------------

// static
std::string ConfigurationPolicyHandler::ValueTypeToString(Value::Type type) {
  static const char* strings[] = {
    "null",
    "boolean",
    "integer",
    "double",
    "string",
    "binary",
    "dictionary",
    "list"
  };
  CHECK(static_cast<size_t>(type) < arraysize(strings));
  return std::string(strings[type]);
}

ConfigurationPolicyHandler::ConfigurationPolicyHandler() {
}

ConfigurationPolicyHandler::~ConfigurationPolicyHandler() {
}

void ConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  // jstemplate can't render DictionaryValues/objects. Convert those values to
  // a string representation.
  base::DictionaryValue* dict;
  base::ListValue* list;
  for (PolicyMap::const_iterator it = policies->begin();
       it != policies->end(); ++it) {
    const PolicyMap::Entry& entry = it->second;
    if (entry.value->GetAsDictionary(&dict)) {
      base::StringValue* value = DictionaryToJSONString(dict);
      policies->Set(it->first, entry.level, entry.scope,
                    value, entry.external_data_fetcher ?
                        new ExternalDataFetcher(*entry.external_data_fetcher) :
                        NULL);
    } else if (entry.value->GetAsList(&list)) {
      for (size_t i = 0; i < list->GetSize(); ++i) {
        if (list->GetDictionary(i, &dict)) {
          list->Set(i, DictionaryToJSONString(dict));
        }
      }
    }
  }
}


// TypeCheckingPolicyHandler implementation ------------------------------------

TypeCheckingPolicyHandler::TypeCheckingPolicyHandler(
    const char* policy_name,
    Value::Type value_type)
    : policy_name_(policy_name),
      value_type_(value_type) {
}

TypeCheckingPolicyHandler::~TypeCheckingPolicyHandler() {
}

const char* TypeCheckingPolicyHandler::policy_name() const {
  return policy_name_;
}

bool TypeCheckingPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const Value* value = NULL;
  return CheckAndGetValue(policies, errors, &value);
}

bool TypeCheckingPolicyHandler::CheckAndGetValue(const PolicyMap& policies,
                                                 PolicyErrorMap* errors,
                                                 const Value** value) {
  *value = policies.GetValue(policy_name_);
  if (*value && !(*value)->IsType(value_type_)) {
    errors->AddError(policy_name_,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(value_type_));
    return false;
  }
  return true;
}


// IntRangePolicyHandlerBase implementation ------------------------------------

IntRangePolicyHandlerBase::IntRangePolicyHandlerBase(
    const char* policy_name,
    int min,
    int max,
    bool clamp)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_INTEGER),
      min_(min),
      max_(max),
      clamp_(clamp) {
}

bool IntRangePolicyHandlerBase::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
      EnsureInRange(value, NULL, errors);
}

IntRangePolicyHandlerBase::~IntRangePolicyHandlerBase() {
}

bool IntRangePolicyHandlerBase::EnsureInRange(const base::Value* input,
                                              int* output,
                                              PolicyErrorMap* errors) {
  if (!input)
    return true;

  int value;
  if (!input->GetAsInteger(&value)) {
    NOTREACHED();
    return false;
  }

  if (value < min_ || value > max_) {
    if (errors) {
      errors->AddError(policy_name(),
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::IntToString(value));
    }

    if (!clamp_)
      return false;

    value = std::min(std::max(value, min_), max_);
  }

  if (output)
    *output = value;
  return true;
}


// StringToIntEnumListPolicyHandler implementation -----------------------------

StringToIntEnumListPolicyHandler::StringToIntEnumListPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    const MappingEntry* mapping_begin,
    const MappingEntry* mapping_end)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
      pref_path_(pref_path),
      mapping_begin_(mapping_begin),
      mapping_end_(mapping_end) {}

bool StringToIntEnumListPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
      Convert(value, NULL, errors);
}

void StringToIntEnumListPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  scoped_ptr<base::ListValue> list(new base::ListValue());
  if (value && Convert(value, list.get(), NULL))
    prefs->SetValue(pref_path_, list.release());
}

bool StringToIntEnumListPolicyHandler::Convert(const base::Value* input,
                                               base::ListValue* output,
                                               PolicyErrorMap* errors) {
  if (!input)
    return true;

  const base::ListValue* list_value = NULL;
  if (!input->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  for (base::ListValue::const_iterator entry(list_value->begin());
       entry != list_value->end(); ++entry) {
    std::string entry_value;
    if (!(*entry)->GetAsString(&entry_value)) {
      if (errors) {
        errors->AddError(policy_name(),
                         entry - list_value->begin(),
                         IDS_POLICY_TYPE_ERROR,
                         ValueTypeToString(base::Value::TYPE_STRING));
      }
      continue;
    }
    bool found = false;
    for (const MappingEntry* mapping_entry(mapping_begin_);
         mapping_entry != mapping_end_; ++mapping_entry) {
      if (mapping_entry->enum_value == entry_value) {
        found = true;
        if (output)
          output->AppendInteger(mapping_entry->int_value);
        break;
      }
    }
    if (!found) {
      if (errors) {
        errors->AddError(policy_name(),
                         entry - list_value->begin(),
                         IDS_POLICY_OUT_OF_RANGE_ERROR);
      }
    }
  }

  return true;
}


// IntRangePolicyHandler implementation ----------------------------------------

IntRangePolicyHandler::IntRangePolicyHandler(const char* policy_name,
                                             const char* pref_path,
                                             int min,
                                             int max,
                                             bool clamp)
    : IntRangePolicyHandlerBase(policy_name, min, max, clamp),
      pref_path_(pref_path) {
}

IntRangePolicyHandler::~IntRangePolicyHandler() {
}

void IntRangePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, NULL)) {
    prefs->SetValue(pref_path_,
                    base::Value::CreateIntegerValue(value_in_range));
  }
}


// IntPercentageToDoublePolicyHandler implementation ---------------------------

IntPercentageToDoublePolicyHandler::IntPercentageToDoublePolicyHandler(
    const char* policy_name,
    const char* pref_path,
    int min,
    int max,
    bool clamp)
    : IntRangePolicyHandlerBase(policy_name, min, max, clamp),
      pref_path_(pref_path) {
}

IntPercentageToDoublePolicyHandler::~IntPercentageToDoublePolicyHandler() {
}

void IntPercentageToDoublePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  int percentage;
  if (value && EnsureInRange(value, &percentage, NULL)) {
    prefs->SetValue(pref_path_, base::Value::CreateDoubleValue(
        static_cast<double>(percentage) / 100.));
  }
}


// ExtensionListPolicyHandler implementation -----------------------------------

ExtensionListPolicyHandler::ExtensionListPolicyHandler(const char* policy_name,
                                                       const char* pref_path,
                                                       bool allow_wildcards)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

ExtensionListPolicyHandler::~ExtensionListPolicyHandler() {}

bool ExtensionListPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return CheckAndGetList(policies, errors, NULL);
}

void ExtensionListPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  scoped_ptr<base::ListValue> list;
  PolicyErrorMap errors;
  if (CheckAndGetList(policies, &errors, &list) && list)
    prefs->SetValue(pref_path(), list.release());
}

const char* ExtensionListPolicyHandler::pref_path() const {
  return pref_path_;
}

bool ExtensionListPolicyHandler::CheckAndGetList(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
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
    filtered_list->Append(base::Value::CreateStringValue(id));
  }

  if (extension_ids)
    *extension_ids = filtered_list.Pass();

  return true;
}


// ExtensionInstallForcelistPolicyHandler implementation -----------------------

ExtensionInstallForcelistPolicyHandler::ExtensionInstallForcelistPolicyHandler(
    const char* pref_name)
    : TypeCheckingPolicyHandler(key::kExtensionInstallForcelist,
                                base::Value::TYPE_LIST),
      pref_name_(pref_name) {}

ExtensionInstallForcelistPolicyHandler::
    ~ExtensionInstallForcelistPolicyHandler() {}

bool ExtensionInstallForcelistPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
      ParseList(value, NULL, errors);
}

void ExtensionInstallForcelistPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = NULL;
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  if (CheckAndGetValue(policies, NULL, &value) &&
      value &&
      ParseList(value, dict.get(), NULL)) {
    prefs->SetValue(pref_name_, dict.release());
  }
}

bool ExtensionInstallForcelistPolicyHandler::ParseList(
    const base::Value* policy_value,
    base::DictionaryValue* extension_dict,
    PolicyErrorMap* errors) {
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
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
      pref_path_(pref_path) {}

ExtensionURLPatternListPolicyHandler::~ExtensionURLPatternListPolicyHandler() {}

bool ExtensionURLPatternListPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
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
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->DeepCopy());
}


// SimplePolicyHandler implementation ------------------------------------------

SimplePolicyHandler::SimplePolicyHandler(
    const char* policy_name,
    const char* pref_path,
    Value::Type value_type)
    : TypeCheckingPolicyHandler(policy_name, value_type),
      pref_path_(pref_path) {
}

SimplePolicyHandler::~SimplePolicyHandler() {
}

void SimplePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                              PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->DeepCopy());
}


// SyncPolicyHandler implementation --------------------------------------------

SyncPolicyHandler::SyncPolicyHandler(const char* pref_name)
    : TypeCheckingPolicyHandler(key::kSyncDisabled, Value::TYPE_BOOLEAN),
      pref_name_(pref_name) {}

SyncPolicyHandler::~SyncPolicyHandler() {
}

void SyncPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  bool disable_sync;
  if (value && value->GetAsBoolean(&disable_sync) && disable_sync)
    prefs->SetValue(pref_name_, value->DeepCopy());
}


// AutofillPolicyHandler implementation ----------------------------------------

AutofillPolicyHandler::AutofillPolicyHandler(const char* pref_name)
    : TypeCheckingPolicyHandler(key::kAutoFillEnabled, Value::TYPE_BOOLEAN),
      pref_name_(pref_name) {}

AutofillPolicyHandler::~AutofillPolicyHandler() {
}

void AutofillPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  bool auto_fill_enabled;
  if (value && value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled)
    prefs->SetValue(pref_name_, Value::CreateBooleanValue(false));
}

// Android doesn't support these policies, and doesn't have a policy_path_parser
// implementation.
#if !defined(OS_ANDROID)

// DiskCacheDirPolicyHandler implementation ------------------------------------

DiskCacheDirPolicyHandler::DiskCacheDirPolicyHandler(const char* pref_name)
    : TypeCheckingPolicyHandler(key::kDiskCacheDir, Value::TYPE_STRING),
      pref_name_(pref_name) {}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {
}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  base::FilePath::StringType string_value;
  if (value && value->GetAsString(&string_value)) {
    base::FilePath::StringType expanded_value =
        policy::path_parser::ExpandPathVariables(string_value);
    prefs->SetValue(pref_name_, Value::CreateStringValue(expanded_value));
  }
}

#endif  // !defined(OS_ANDROID)


// FileSelectionDialogsHandler implementation ----------------------------------

FileSelectionDialogsHandler::FileSelectionDialogsHandler(
    const char* allow_dialogs_pref_name,
    const char* prompt_for_download_pref_name)
    : TypeCheckingPolicyHandler(key::kAllowFileSelectionDialogs,
                                Value::TYPE_BOOLEAN),
      allow_dialogs_pref_name_(allow_dialogs_pref_name),
      prompt_for_download_pref_name_(prompt_for_download_pref_name) {}

FileSelectionDialogsHandler::~FileSelectionDialogsHandler() {
}

void FileSelectionDialogsHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  bool allow_dialogs;
  const Value* value = policies.GetValue(policy_name());
  if (value && value->GetAsBoolean(&allow_dialogs)) {
    prefs->SetValue(allow_dialogs_pref_name_,
                    Value::CreateBooleanValue(allow_dialogs));
    // Disallow selecting the download location if file dialogs are disabled.
    if (!allow_dialogs) {
      prefs->SetValue(prompt_for_download_pref_name_,
                      Value::CreateBooleanValue(false));
    }
  }
}


// IncognitoModePolicyHandler implementation -----------------------------------

IncognitoModePolicyHandler::IncognitoModePolicyHandler(const char* pref_name)
    : pref_name_(pref_name) {}

IncognitoModePolicyHandler::~IncognitoModePolicyHandler() {
}

bool IncognitoModePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  int int_value = IncognitoModePrefs::ENABLED;
  const Value* availability =
      policies.GetValue(key::kIncognitoModeAvailability);

  if (availability) {
    if (availability->GetAsInteger(&int_value)) {
      IncognitoModePrefs::Availability availability_enum_value;
      if (!IncognitoModePrefs::IntToAvailability(int_value,
                                                 &availability_enum_value)) {
        errors->AddError(key::kIncognitoModeAvailability,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::IntToString(int_value));
        return false;
      }
    } else {
      errors->AddError(key::kIncognitoModeAvailability,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }
  } else {
    const Value* deprecated_enabled = policies.GetValue(key::kIncognitoEnabled);
    if (deprecated_enabled &&
        !deprecated_enabled->IsType(Value::TYPE_BOOLEAN)) {
      errors->AddError(key::kIncognitoEnabled,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }
  }
  return true;
}

void IncognitoModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const Value* availability =
      policies.GetValue(key::kIncognitoModeAvailability);
  const Value* deprecated_enabled = policies.GetValue(key::kIncognitoEnabled);
  if (availability) {
    int int_value = IncognitoModePrefs::ENABLED;
    IncognitoModePrefs::Availability availability_enum_value;
    if (availability->GetAsInteger(&int_value) &&
        IncognitoModePrefs::IntToAvailability(int_value,
                                              &availability_enum_value)) {
      prefs->SetValue(pref_name_,
                      Value::CreateIntegerValue(availability_enum_value));
    } else {
      NOTREACHED();
    }
  } else if (deprecated_enabled) {
    // If kIncognitoModeAvailability is not specified, check the obsolete
    // kIncognitoEnabled.
    bool enabled = true;
    if (deprecated_enabled->GetAsBoolean(&enabled)) {
      prefs->SetInteger(
          pref_name_,
          enabled ? IncognitoModePrefs::ENABLED : IncognitoModePrefs::DISABLED);
    } else {
      NOTREACHED();
    }
  }
}


// JavascriptPolicyHandler implementation --------------------------------------

JavascriptPolicyHandler::JavascriptPolicyHandler(const char* pref_name)
    : pref_name_(pref_name) {}

JavascriptPolicyHandler::~JavascriptPolicyHandler() {
}

bool JavascriptPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  const Value* javascript_enabled = policies.GetValue(key::kJavascriptEnabled);
  const Value* default_setting =
      policies.GetValue(key::kDefaultJavaScriptSetting);

  if (javascript_enabled && !javascript_enabled->IsType(Value::TYPE_BOOLEAN)) {
    errors->AddError(key::kJavascriptEnabled,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_BOOLEAN));
  }

  if (default_setting && !default_setting->IsType(Value::TYPE_INTEGER)) {
    errors->AddError(key::kDefaultJavaScriptSetting,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_INTEGER));
  }

  if (javascript_enabled && default_setting) {
    errors->AddError(key::kJavascriptEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     key::kDefaultJavaScriptSetting);
  }

  return true;
}

void JavascriptPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  int setting = CONTENT_SETTING_DEFAULT;
  const Value* default_setting =
      policies.GetValue(key::kDefaultJavaScriptSetting);

  if (default_setting) {
    default_setting->GetAsInteger(&setting);
  } else {
    const Value* javascript_enabled =
        policies.GetValue(key::kJavascriptEnabled);
    bool enabled = true;
    if (javascript_enabled &&
        javascript_enabled->GetAsBoolean(&enabled) &&
        !enabled) {
      setting = CONTENT_SETTING_BLOCK;
    }
  }

  if (setting != CONTENT_SETTING_DEFAULT)
    prefs->SetValue(pref_name_, Value::CreateIntegerValue(setting));
}


// URLBlacklistPolicyHandler implementation ------------------------------------

URLBlacklistPolicyHandler::URLBlacklistPolicyHandler(const char* pref_name)
    : pref_name_(pref_name) {}

URLBlacklistPolicyHandler::~URLBlacklistPolicyHandler() {
}

bool URLBlacklistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const Value* disabled_schemes = policies.GetValue(key::kDisabledSchemes);
  const Value* url_blacklist = policies.GetValue(key::kURLBlacklist);

  if (disabled_schemes && !disabled_schemes->IsType(Value::TYPE_LIST)) {
    errors->AddError(key::kDisabledSchemes,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_LIST));
  }

  if (url_blacklist && !url_blacklist->IsType(Value::TYPE_LIST)) {
      errors->AddError(key::kURLBlacklist,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_LIST));
  }

  return true;
}

void URLBlacklistPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* url_blacklist_policy =
      policies.GetValue(key::kURLBlacklist);
  const base::ListValue* url_blacklist = NULL;
  if (url_blacklist_policy)
    url_blacklist_policy->GetAsList(&url_blacklist);
  const base::Value* disabled_schemes_policy =
      policies.GetValue(key::kDisabledSchemes);
  const base::ListValue* disabled_schemes = NULL;
  if (disabled_schemes_policy)
    disabled_schemes_policy->GetAsList(&disabled_schemes);

  scoped_ptr<base::ListValue> merged_url_blacklist(new base::ListValue());

  // We start with the DisabledSchemes because we have size limit when
  // handling URLBacklists.
  if (disabled_schemes_policy) {
    for (base::ListValue::const_iterator entry(disabled_schemes->begin());
         entry != disabled_schemes->end(); ++entry) {
      std::string entry_value;
      if ((*entry)->GetAsString(&entry_value)) {
        entry_value.append("://*");
        merged_url_blacklist->AppendString(entry_value);
      }
    }
  }

  if (url_blacklist_policy) {
    for (base::ListValue::const_iterator entry(url_blacklist->begin());
         entry != url_blacklist->end(); ++entry) {
      if ((*entry)->IsType(Value::TYPE_STRING))
        merged_url_blacklist->Append((*entry)->DeepCopy());
    }
  }

  if (disabled_schemes_policy || url_blacklist_policy)
    prefs->SetValue(pref_name_, merged_url_blacklist.release());
}


// RestoreOnStartupPolicyHandler implementation --------------------------------

RestoreOnStartupPolicyHandler::RestoreOnStartupPolicyHandler(
    const char* restore_on_startup_pref_name,
    const char* startup_url_list_pref_name)
    : TypeCheckingPolicyHandler(key::kRestoreOnStartup, Value::TYPE_INTEGER),
      restore_on_startup_pref_name_(restore_on_startup_pref_name),
      startup_url_list_pref_name_(startup_url_list_pref_name) {}

RestoreOnStartupPolicyHandler::~RestoreOnStartupPolicyHandler() {
}

void RestoreOnStartupPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const Value* restore_on_startup_value = policies.GetValue(policy_name());
  if (restore_on_startup_value) {
    int restore_on_startup;
    if (!restore_on_startup_value->GetAsInteger(&restore_on_startup))
      return;

    if (restore_on_startup == SessionStartupPref::kPrefValueHomePage)
      ApplyPolicySettingsFromHomePage(policies, prefs);
    else
      prefs->SetInteger(restore_on_startup_pref_name_, restore_on_startup);
  }
}

void RestoreOnStartupPolicyHandler::ApplyPolicySettingsFromHomePage(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* homepage_is_new_tab_page_value =
      policies.GetValue(key::kHomepageIsNewTabPage);
  if (!homepage_is_new_tab_page_value) {
    // The policy is enforcing 'open the homepage on startup' but not
    // enforcing what the homepage should be. Don't set any prefs.
    return;
  }

  bool homepage_is_new_tab_page;
  if (!homepage_is_new_tab_page_value->GetAsBoolean(&homepage_is_new_tab_page))
    return;

  if (homepage_is_new_tab_page) {
    prefs->SetInteger(restore_on_startup_pref_name_,
                      SessionStartupPref::kPrefValueNewTab);
  } else {
    const base::Value* homepage_value =
        policies.GetValue(key::kHomepageLocation);
    if (!homepage_value || !homepage_value->IsType(base::Value::TYPE_STRING)) {
      // The policy is enforcing 'open the homepage on startup' but not
      // enforcing what the homepage should be. Don't set any prefs.
      return;
    }
    ListValue* url_list = new ListValue();
    url_list->Append(homepage_value->DeepCopy());
    prefs->SetInteger(restore_on_startup_pref_name_,
                      SessionStartupPref::kPrefValueURLs);
    prefs->SetValue(startup_url_list_pref_name_, url_list);
  }
}

bool RestoreOnStartupPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const base::Value* restore_policy = policies.GetValue(key::kRestoreOnStartup);

  if (restore_policy) {
    int restore_value;
    if (restore_policy->GetAsInteger(&restore_value)) {
      switch (restore_value) {
        case SessionStartupPref::kPrefValueHomePage:
          errors->AddError(policy_name(), IDS_POLICY_VALUE_DEPRECATED);
          break;
        case SessionStartupPref::kPrefValueLast: {
          // If the "restore last session" policy is set, session cookies are
          // treated as permanent cookies and site data needed to restore the
          // session is not cleared so we have to warn the user in that case.
          const base::Value* cookies_policy =
              policies.GetValue(key::kCookiesSessionOnlyForUrls);
          const base::ListValue *cookies_value;
          if (cookies_policy && cookies_policy->GetAsList(&cookies_value) &&
              !cookies_value->empty()) {
            errors->AddError(key::kCookiesSessionOnlyForUrls,
                             IDS_POLICY_OVERRIDDEN,
                             key::kRestoreOnStartup);
          }

          const base::Value* exit_policy =
              policies.GetValue(key::kClearSiteDataOnExit);
          bool exit_value;
          if (exit_policy &&
              exit_policy->GetAsBoolean(&exit_value) && exit_value) {
            errors->AddError(key::kClearSiteDataOnExit,
                             IDS_POLICY_OVERRIDDEN,
                             key::kRestoreOnStartup);
          }
          break;
        }
        case SessionStartupPref::kPrefValueURLs:
        case SessionStartupPref::kPrefValueNewTab:
          // No error
          break;
        default:
          errors->AddError(policy_name(),
                           IDS_POLICY_OUT_OF_RANGE_ERROR,
                           base::IntToString(restore_value));
      }
    }
  }
  return true;
}

}  // namespace policy

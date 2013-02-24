// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"

#include <algorithm>
#include <string>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/prefs/pref_value_map.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Helper classes --------------------------------------------------------------

// This is used to check whether for a given ProxyMode value, the ProxyPacUrl,
// the ProxyBypassList and the ProxyServer policies are allowed to be specified.
// |error_message_id| is the message id of the localized error message to show
// when the policies are not specified as allowed. Each value of ProxyMode
// has a ProxyModeValidationEntry in the |kProxyModeValidationMap| below.
struct ProxyModeValidationEntry {
  const char* mode_value;
  bool pac_url_allowed;
  bool bypass_list_allowed;
  bool server_allowed;
  int error_message_id;
};

// Maps a policy type to a preference path, and to the expected value type.
struct DefaultSearchSimplePolicyHandlerEntry {
  const char* policy_name;
  const char* preference_path;
  base::Value::Type value_type;
};


// Static data -----------------------------------------------------------------

// List of policy types to preference names, for policies affecting the default
// search provider.
const DefaultSearchSimplePolicyHandlerEntry kDefaultSearchPolicyMap[] = {
  { key::kDefaultSearchProviderEnabled,
    prefs::kDefaultSearchProviderEnabled,
    Value::TYPE_BOOLEAN },
  { key::kDefaultSearchProviderName,
    prefs::kDefaultSearchProviderName,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderKeyword,
    prefs::kDefaultSearchProviderKeyword,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderSearchURL,
    prefs::kDefaultSearchProviderSearchURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderSuggestURL,
    prefs::kDefaultSearchProviderSuggestURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderInstantURL,
    prefs::kDefaultSearchProviderInstantURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderIconURL,
    prefs::kDefaultSearchProviderIconURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderEncodings,
    prefs::kDefaultSearchProviderEncodings,
    Value::TYPE_LIST },
  { key::kDefaultSearchProviderAlternateURLs,
    prefs::kDefaultSearchProviderAlternateURLs,
    Value::TYPE_LIST },
  { key::kDefaultSearchProviderSearchTermsReplacementKey,
    prefs::kDefaultSearchProviderSearchTermsReplacementKey,
    Value::TYPE_STRING },
};

// List of entries determining which proxy policies can be specified, depending
// on the ProxyMode.
const ProxyModeValidationEntry kProxyModeValidationMap[] = {
  { ProxyPrefs::kDirectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_DISABLED_ERROR },
  { ProxyPrefs::kAutoDetectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_AUTO_DETECT_ERROR },
  { ProxyPrefs::kPacScriptProxyModeName,
    true, false, false, IDS_POLICY_PROXY_MODE_PAC_URL_ERROR },
  { ProxyPrefs::kFixedServersProxyModeName,
    false, true, true, IDS_POLICY_PROXY_MODE_FIXED_SERVERS_ERROR },
  { ProxyPrefs::kSystemProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_SYSTEM_ERROR },
};


// Helper functions ------------------------------------------------------------

std::string ValueTypeToString(Value::Type type) {
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
      policies->Set(it->first, entry.level, entry.scope, value);
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

ExtensionInstallForcelistPolicyHandler::
    ExtensionInstallForcelistPolicyHandler()
        : TypeCheckingPolicyHandler(key::kExtensionInstallForcelist,
                                    base::Value::TYPE_LIST) {}

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
    prefs->SetValue(prefs::kExtensionInstallForceList, dict.release());
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
  const Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->DeepCopy());
}


// SyncPolicyHandler implementation --------------------------------------------

SyncPolicyHandler::SyncPolicyHandler()
    : TypeCheckingPolicyHandler(key::kSyncDisabled,
                                Value::TYPE_BOOLEAN) {
}

SyncPolicyHandler::~SyncPolicyHandler() {
}

void SyncPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  bool disable_sync;
  if (value && value->GetAsBoolean(&disable_sync) && disable_sync)
    prefs->SetValue(prefs::kSyncManaged, value->DeepCopy());
}


// AutofillPolicyHandler implementation ----------------------------------------

AutofillPolicyHandler::AutofillPolicyHandler()
    : TypeCheckingPolicyHandler(key::kAutoFillEnabled,
                                Value::TYPE_BOOLEAN) {
}

AutofillPolicyHandler::~AutofillPolicyHandler() {
}

void AutofillPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  bool auto_fill_enabled;
  if (value && value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled) {
    prefs->SetValue(prefs::kAutofillEnabled,
                    Value::CreateBooleanValue(false));
  }
}


// DownloadDirPolicyHandler implementation -------------------------------------

DownloadDirPolicyHandler::DownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDownloadDirectory,
                                Value::TYPE_STRING) {
}

DownloadDirPolicyHandler::~DownloadDirPolicyHandler() {
}

void DownloadDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  base::FilePath::StringType string_value;
  if (!value || !value->GetAsString(&string_value))
    return;

  base::FilePath::StringType expanded_value =
      policy::path_parser::ExpandPathVariables(string_value);
  // Make sure the path isn't empty, since that will point to an undefined
  // location; the default location is used instead in that case.
  // This is checked after path expansion because a non-empty policy value can
  // lead to an empty path value after expansion (e.g. "\"\"").
  if (expanded_value.empty())
    expanded_value = download_util::GetDefaultDownloadDirectory().value();
  prefs->SetValue(prefs::kDownloadDefaultDirectory,
                  Value::CreateStringValue(expanded_value));
  prefs->SetValue(prefs::kPromptForDownload,
                  Value::CreateBooleanValue(false));
}


// DiskCacheDirPolicyHandler implementation ------------------------------------

DiskCacheDirPolicyHandler::DiskCacheDirPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDiskCacheDir,
                                Value::TYPE_STRING) {
}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {
}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const Value* value = policies.GetValue(policy_name());
  base::FilePath::StringType string_value;
  if (value && value->GetAsString(&string_value)) {
    base::FilePath::StringType expanded_value =
        policy::path_parser::ExpandPathVariables(string_value);
    prefs->SetValue(prefs::kDiskCacheDir,
                    Value::CreateStringValue(expanded_value));
  }
}


// FileSelectionDialogsHandler implementation ----------------------------------

FileSelectionDialogsHandler::FileSelectionDialogsHandler()
    : TypeCheckingPolicyHandler(key::kAllowFileSelectionDialogs,
                                Value::TYPE_BOOLEAN) {
}

FileSelectionDialogsHandler::~FileSelectionDialogsHandler() {
}

void FileSelectionDialogsHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  bool allow_dialogs;
  const Value* value = policies.GetValue(policy_name());
  if (value && value->GetAsBoolean(&allow_dialogs)) {
    prefs->SetValue(prefs::kAllowFileSelectionDialogs,
                    Value::CreateBooleanValue(allow_dialogs));
    // Disallow selecting the download location if file dialogs are disabled.
    if (!allow_dialogs) {
      prefs->SetValue(prefs::kPromptForDownload,
                      Value::CreateBooleanValue(false));
    }
  }
}


// IncognitoModePolicyHandler implementation -----------------------------------

IncognitoModePolicyHandler::IncognitoModePolicyHandler() {
}

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
      prefs->SetValue(prefs::kIncognitoModeAvailability,
                      Value::CreateIntegerValue(availability_enum_value));
    } else {
      NOTREACHED();
    }
  } else if (deprecated_enabled) {
    // If kIncognitoModeAvailability is not specified, check the obsolete
    // kIncognitoEnabled.
    bool enabled = true;
    if (deprecated_enabled->GetAsBoolean(&enabled)) {
      prefs->SetInteger(prefs::kIncognitoModeAvailability,
                        enabled ? IncognitoModePrefs::ENABLED :
                                  IncognitoModePrefs::DISABLED);
    } else {
      NOTREACHED();
    }
  }
}


// DefaultSearchEncodingsPolicyHandler implementation --------------------------

DefaultSearchEncodingsPolicyHandler::DefaultSearchEncodingsPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDefaultSearchProviderEncodings,
                                Value::TYPE_LIST) {
}

DefaultSearchEncodingsPolicyHandler::~DefaultSearchEncodingsPolicyHandler() {
}

void DefaultSearchEncodingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies, PrefValueMap* prefs) {
  // The DefaultSearchProviderEncodings policy has type list, but the related
  // preference has type string. Convert one into the other here, using
  // ';' as a separator.
  const Value* value = policies.GetValue(policy_name());
  const ListValue* list;
  if (!value || !value->GetAsList(&list))
    return;

  ListValue::const_iterator iter(list->begin());
  ListValue::const_iterator end(list->end());
  std::vector<std::string> string_parts;
  for (; iter != end; ++iter) {
    std::string s;
    if ((*iter)->GetAsString(&s)) {
      string_parts.push_back(s);
    }
  }
  std::string encodings = JoinString(string_parts, ';');
  prefs->SetValue(prefs::kDefaultSearchProviderEncodings,
                  Value::CreateStringValue(encodings));
}


// DefaultSearchPolicyHandler implementation -----------------------------------

DefaultSearchPolicyHandler::DefaultSearchPolicyHandler() {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    const char* policy_name = kDefaultSearchPolicyMap[i].policy_name;
    if (policy_name == key::kDefaultSearchProviderEncodings) {
      handlers_.push_back(new DefaultSearchEncodingsPolicyHandler());
    } else {
      handlers_.push_back(
          new SimplePolicyHandler(policy_name,
                                  kDefaultSearchPolicyMap[i].preference_path,
                                  kDefaultSearchPolicyMap[i].value_type));
    }
  }
}

DefaultSearchPolicyHandler::~DefaultSearchPolicyHandler() {
  STLDeleteElements(&handlers_);
}

bool DefaultSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  if (!CheckIndividualPolicies(policies, errors))
    return false;

  if (DefaultSearchProviderIsDisabled(policies)) {
    // Add an error for all specified default search policies except
    // DefaultSearchProviderEnabled.
    for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
      const char* policy_name = kDefaultSearchPolicyMap[i].policy_name;
      if (policy_name != key::kDefaultSearchProviderEnabled &&
          HasDefaultSearchPolicy(policies, policy_name)) {
        errors->AddError(policy_name, IDS_POLICY_DEFAULT_SEARCH_DISABLED);
      }
    }
    return true;
  }

  const Value* url;
  std::string dummy;
  if (DefaultSearchURLIsValid(policies, &url, &dummy) ||
      !AnyDefaultSearchPoliciesSpecified(policies))
    return true;
  errors->AddError(key::kDefaultSearchProviderSearchURL, url ?
      IDS_POLICY_INVALID_SEARCH_URL_ERROR : IDS_POLICY_NOT_SPECIFIED_ERROR);
  return false;
}

void DefaultSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  if (DefaultSearchProviderIsDisabled(policies)) {
    prefs->SetBoolean(prefs::kDefaultSearchProviderEnabled, false);

    // If default search is disabled, the other fields are ignored.
    prefs->SetString(prefs::kDefaultSearchProviderName, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSearchURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderIconURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderEncodings, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderKeyword, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderInstantURL, std::string());
    prefs->SetValue(prefs::kDefaultSearchProviderAlternateURLs,
                    new ListValue());
    prefs->SetString(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
                     std::string());
  } else {
    // The search URL is required.  The other entries are optional.  Just make
    // sure that they are all specified via policy, so that the regular prefs
    // aren't used.
    const Value* dummy;
    std::string url;
    if (DefaultSearchURLIsValid(policies, &dummy, &url)) {
      for (std::vector<ConfigurationPolicyHandler*>::const_iterator handler =
           handlers_.begin(); handler != handlers_.end(); ++handler)
        (*handler)->ApplyPolicySettings(policies, prefs);

      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderSuggestURL);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderIconURL);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderEncodings);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderKeyword);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderInstantURL);
      EnsureListPrefExists(prefs, prefs::kDefaultSearchProviderAlternateURLs);
      EnsureStringPrefExists(prefs,
          prefs::kDefaultSearchProviderSearchTermsReplacementKey);

      // For the name and keyword, default to the host if not specified.  If
      // there is no host (file: URLs?  Not sure), use "_" to guarantee that the
      // keyword is non-empty.
      std::string name, keyword;
      std::string host(GURL(url).host());
      if (host.empty())
        host = "_";
      if (!prefs->GetString(prefs::kDefaultSearchProviderName, &name) ||
          name.empty())
        prefs->SetString(prefs::kDefaultSearchProviderName, host);
      if (!prefs->GetString(prefs::kDefaultSearchProviderKeyword, &keyword) ||
          keyword.empty())
        prefs->SetString(prefs::kDefaultSearchProviderKeyword, host);

      // And clear the IDs since these are not specified via policy.
      prefs->SetString(prefs::kDefaultSearchProviderID, std::string());
      prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID,
                       std::string());
    }
  }
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

}

bool DefaultSearchPolicyHandler::CheckIndividualPolicies(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin() ; handler != handlers_.end(); ++handler) {
    if (!(*handler)->CheckPolicySettings(policies, errors))
      return false;
  }
  return true;
}

bool DefaultSearchPolicyHandler::HasDefaultSearchPolicy(
    const PolicyMap& policies,
    const char* policy_name) {
  return policies.Get(policy_name) != NULL;
}

bool DefaultSearchPolicyHandler::AnyDefaultSearchPoliciesSpecified(
    const PolicyMap& policies) {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    if (policies.Get(kDefaultSearchPolicyMap[i].policy_name))
      return true;
  }
  return false;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderIsDisabled(
    const PolicyMap& policies) {
  const Value* provider_enabled =
      policies.GetValue(key::kDefaultSearchProviderEnabled);
  bool enabled = true;
  return provider_enabled && provider_enabled->GetAsBoolean(&enabled) &&
      !enabled;
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies,
    const Value** url_value,
    std::string* url_string) {
  *url_value = policies.GetValue(key::kDefaultSearchProviderSearchURL);
  if (!*url_value || !(*url_value)->GetAsString(url_string) ||
      url_string->empty())
    return false;
  TemplateURLData data;
  data.SetURL(*url_string);
  SearchTermsData search_terms_data;
  return TemplateURL(NULL, data).SupportsReplacementUsingTermsData(
      search_terms_data);
}

void DefaultSearchPolicyHandler::EnsureStringPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  std::string value;
  if (!prefs->GetString(path, &value))
    prefs->SetString(path, value);
}

void DefaultSearchPolicyHandler::EnsureListPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  base::Value* value;
  base::ListValue* list_value;
  if (!prefs->GetValue(path, &value) || !value->GetAsList(&list_value))
    prefs->SetValue(path, new ListValue());
}


// ProxyPolicyHandler implementation -------------------------------------------

// The proxy policies have the peculiarity that they are loaded from individual
// policies, but the providers then expose them through a unified
// DictionaryValue. Once Dictionary policies are fully supported, the individual
// proxy policies will be deprecated. http://crbug.com/108996

ProxyPolicyHandler::ProxyPolicyHandler() {
}

ProxyPolicyHandler::~ProxyPolicyHandler() {
}

bool ProxyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                             PolicyErrorMap* errors) {
  const Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, key::kProxyBypassList);

  if ((server || pac_url || bypass_list) && !(mode || server_mode)) {
    errors->AddError(key::kProxySettings,
                     key::kProxyMode,
                     IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }

  std::string mode_value;
  if (!CheckProxyModeAndServerMode(policies, errors, &mode_value))
    return false;

  // If neither ProxyMode nor ProxyServerMode are specified, mode_value will be
  // empty and the proxy shouldn't be configured at all.
  if (mode_value.empty())
    return true;

  bool is_valid_mode = false;
  for (size_t i = 0; i != arraysize(kProxyModeValidationMap); ++i) {
    const ProxyModeValidationEntry& entry = kProxyModeValidationMap[i];
    if (entry.mode_value != mode_value)
      continue;

    is_valid_mode = true;

    if (!entry.pac_url_allowed && pac_url) {
      errors->AddError(key::kProxySettings,
                       key::kProxyPacUrl,
                       entry.error_message_id);
    }
    if (!entry.bypass_list_allowed && bypass_list) {
      errors->AddError(key::kProxySettings,
                       key::kProxyBypassList,
                       entry.error_message_id);
    }
    if (!entry.server_allowed && server) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServer,
                       entry.error_message_id);
    }

    if ((!entry.pac_url_allowed && pac_url) ||
        (!entry.bypass_list_allowed && bypass_list) ||
        (!entry.server_allowed && server)) {
      return false;
    }
  }

  if (!is_valid_mode) {
    errors->AddError(key::kProxySettings,
                     mode ? key::kProxyMode : key::kProxyServerMode,
                     IDS_POLICY_OUT_OF_RANGE_ERROR,
                     mode_value);
    return false;
  }
  return true;
}

void ProxyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                             PrefValueMap* prefs) {
  const Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, key::kProxyBypassList);

  ProxyPrefs::ProxyMode proxy_mode;
  if (mode) {
    std::string string_mode;
    CHECK(mode->GetAsString(&string_mode));
    CHECK(ProxyPrefs::StringToProxyMode(string_mode, &proxy_mode));
  } else if (server_mode) {
    int int_mode = 0;
    CHECK(server_mode->GetAsInteger(&int_mode));

    switch (int_mode) {
      case PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        break;
      case PROXY_AUTO_DETECT_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_AUTO_DETECT;
        break;
      case PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_FIXED_SERVERS;
        if (pac_url)
          proxy_mode = ProxyPrefs::MODE_PAC_SCRIPT;
        break;
      case PROXY_USE_SYSTEM_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_SYSTEM;
        break;
      default:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        NOTREACHED();
    }
  } else {
    return;
  }

  switch (proxy_mode) {
    case ProxyPrefs::MODE_DIRECT:
      prefs->SetValue(prefs::kProxy, ProxyConfigDictionary::CreateDirect());
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      prefs->SetValue(prefs::kProxy, ProxyConfigDictionary::CreateAutoDetect());
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url_string;
      if (pac_url && pac_url->GetAsString(&pac_url_string)) {
        prefs->SetValue(prefs::kProxy,
            ProxyConfigDictionary::CreatePacScript(pac_url_string, false));
      } else {
        NOTREACHED();
      }
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      std::string bypass_list_string;
      if (server->GetAsString(&proxy_server)) {
        if (bypass_list)
          bypass_list->GetAsString(&bypass_list_string);
        prefs->SetValue(prefs::kProxy,
                        ProxyConfigDictionary::CreateFixedServers(
                            proxy_server, bypass_list_string));
      }
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      prefs->SetValue(prefs::kProxy,
                      ProxyConfigDictionary::CreateSystem());
      break;
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
}

const Value* ProxyPolicyHandler::GetProxyPolicyValue(
    const PolicyMap& policies, const char* policy_name) {
  // See note on the ProxyPolicyHandler implementation above.
  const Value* value = policies.GetValue(key::kProxySettings);
  const DictionaryValue* settings;
  if (!value || !value->GetAsDictionary(&settings))
    return NULL;

  const Value* policy_value = NULL;
  std::string tmp;
  if (!settings->Get(policy_name, &policy_value) ||
      policy_value->IsType(Value::TYPE_NULL) ||
      (policy_value->IsType(Value::TYPE_STRING) &&
       policy_value->GetAsString(&tmp) &&
       tmp.empty())) {
    return NULL;
  }
  return policy_value;
}

bool ProxyPolicyHandler::CheckProxyModeAndServerMode(const PolicyMap& policies,
                                                     PolicyErrorMap* errors,
                                                     std::string* mode_value) {
  const Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);

  // If there's a server mode, convert it into a mode.
  // When both are specified, the mode takes precedence.
  if (mode) {
    if (server_mode) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServerMode,
                       IDS_POLICY_OVERRIDDEN,
                       key::kProxyMode);
    }
    if (!mode->GetAsString(mode_value)) {
      errors->AddError(key::kProxySettings,
                       key::kProxyMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }

    ProxyPrefs::ProxyMode mode;
    if (!ProxyPrefs::StringToProxyMode(*mode_value, &mode)) {
      errors->AddError(key::kProxySettings,
                       key::kProxyMode,
                       IDS_POLICY_INVALID_PROXY_MODE_ERROR);
      return false;
    }

    if (mode == ProxyPrefs::MODE_PAC_SCRIPT && !pac_url) {
      errors->AddError(key::kProxySettings,
                       key::kProxyPacUrl,
                       IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    } else if (mode == ProxyPrefs::MODE_FIXED_SERVERS && !server) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServer,
                       IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    }
  } else if (server_mode) {
    int server_mode_value;
    if (!server_mode->GetAsInteger(&server_mode_value)) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServerMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }

    switch (server_mode_value) {
      case PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kDirectProxyModeName;
        break;
      case PROXY_AUTO_DETECT_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kAutoDetectProxyModeName;
        break;
      case PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE:
        if (server && pac_url) {
          int message_id = IDS_POLICY_PROXY_BOTH_SPECIFIED_ERROR;
          errors->AddError(key::kProxySettings,
                           key::kProxyServer,
                           message_id);
          errors->AddError(key::kProxySettings,
                           key::kProxyPacUrl,
                           message_id);
          return false;
        }
        if (!server && !pac_url) {
          int message_id = IDS_POLICY_PROXY_NEITHER_SPECIFIED_ERROR;
          errors->AddError(key::kProxySettings,
                           key::kProxyServer,
                           message_id);
          errors->AddError(key::kProxySettings,
                           key::kProxyPacUrl,
                           message_id);
          return false;
        }
        if (pac_url)
          *mode_value = ProxyPrefs::kPacScriptProxyModeName;
        else
          *mode_value = ProxyPrefs::kFixedServersProxyModeName;
        break;
      case PROXY_USE_SYSTEM_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kSystemProxyModeName;
        break;
      default:
        errors->AddError(key::kProxySettings,
                         key::kProxyServerMode,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::IntToString(server_mode_value));
        return false;
    }
  }
  return true;
}


// JavascriptPolicyHandler implementation --------------------------------------

JavascriptPolicyHandler::JavascriptPolicyHandler() {
}

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

  if (setting != CONTENT_SETTING_DEFAULT) {
    prefs->SetValue(prefs::kManagedDefaultJavaScriptSetting,
                    Value::CreateIntegerValue(setting));
  }
}

// ClearSiteDataOnExitPolicyHandler implementation -----------------------------

ClearSiteDataOnExitPolicyHandler::ClearSiteDataOnExitPolicyHandler()
    : TypeCheckingPolicyHandler(key::kClearSiteDataOnExit,
                                Value::TYPE_BOOLEAN) {
}

ClearSiteDataOnExitPolicyHandler::~ClearSiteDataOnExitPolicyHandler() {
}

bool ClearSiteDataOnExitPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
  if (ClearSiteDataEnabled(policies) &&
      GetContentSetting(policies, &content_setting) &&
      content_setting == CONTENT_SETTING_ALLOW) {
    errors->AddError(key::kDefaultCookiesSetting,
                     IDS_POLICY_OVERRIDDEN,
                     policy_name());
  }

  return TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors);
}

void ClearSiteDataOnExitPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (ClearSiteDataEnabled(policies)) {
    ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
    if (!GetContentSetting(policies, &content_setting) ||
        content_setting == CONTENT_SETTING_ALLOW) {
      prefs->SetValue(
          prefs::kManagedDefaultCookiesSetting,
          Value::CreateIntegerValue(CONTENT_SETTING_SESSION_ONLY));
    }
  }
}

bool ClearSiteDataOnExitPolicyHandler::ClearSiteDataEnabled(
    const PolicyMap& policies) {
  const base::Value* value = NULL;
  PolicyErrorMap errors;
  bool clear_site_data = false;

  return (CheckAndGetValue(policies, &errors, &value) &&
          value &&
          value->GetAsBoolean(&clear_site_data) &&
          clear_site_data);
}

// static
bool ClearSiteDataOnExitPolicyHandler::GetContentSetting(
    const PolicyMap& policies,
    ContentSetting* content_setting) {
  const base::Value* value = policies.GetValue(key::kDefaultCookiesSetting);
  int setting = CONTENT_SETTING_DEFAULT;
  if (value && value->GetAsInteger(&setting)) {
    *content_setting = static_cast<ContentSetting>(setting);
    return true;
  }

  return false;
}

// RestoreOnStartupPolicyHandler implementation --------------------------------

RestoreOnStartupPolicyHandler::RestoreOnStartupPolicyHandler()
    : TypeCheckingPolicyHandler(key::kRestoreOnStartup,
                                Value::TYPE_INTEGER) {
}

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
      prefs->SetInteger(prefs::kRestoreOnStartup, restore_on_startup);
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
    prefs->SetInteger(prefs::kRestoreOnStartup,
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
    prefs->SetInteger(prefs::kRestoreOnStartup,
                      SessionStartupPref::kPrefValueURLs);
    prefs->SetValue(prefs::kURLsToRestoreOnStartup, url_list);
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

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"

#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Helper classes --------------------------------------------------------------

// Implementation of SearchTermsData just for validation.
class SearchTermsDataForValidation : public SearchTermsData {
 public:
  SearchTermsDataForValidation() {}

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const {
    return "http://www.google.com/";
  }
  virtual std::string GetApplicationLocale() const {
    return "en";
  }
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual string16 GetRlzParameterValue() const {
    return string16();
  }
#endif
 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsDataForValidation);
};

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
  base::Value::Type value_type;
  ConfigurationPolicyType policy_type;
  const char* preference_path;
};


// Static data -----------------------------------------------------------------

// List of policy types to preference names, for policies affecting the default
// search provider.
const DefaultSearchSimplePolicyHandlerEntry kDefaultSearchPolicyMap[] = {
  { Value::TYPE_BOOLEAN, kPolicyDefaultSearchProviderEnabled,
    prefs::kDefaultSearchProviderEnabled },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderName,
    prefs::kDefaultSearchProviderName },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderKeyword,
    prefs::kDefaultSearchProviderKeyword },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderSearchURL,
    prefs::kDefaultSearchProviderSearchURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderSuggestURL,
    prefs::kDefaultSearchProviderSuggestURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderInstantURL,
    prefs::kDefaultSearchProviderInstantURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderIconURL,
    prefs::kDefaultSearchProviderIconURL },
  { Value::TYPE_LIST, kPolicyDefaultSearchProviderEncodings,
    prefs::kDefaultSearchProviderEncodings },
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
  DCHECK(static_cast<size_t>(type) < arraysize(strings));
  return std::string(strings[type]);
}


}  // namespace


// ConfigurationPolicyHandler implementation -----------------------------------

ConfigurationPolicyHandler::ConfigurationPolicyHandler() {
}

ConfigurationPolicyHandler::~ConfigurationPolicyHandler() {
}

void ConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
}


// TypeCheckingPolicyHandler implementation ------------------------------------

TypeCheckingPolicyHandler::TypeCheckingPolicyHandler(
    ConfigurationPolicyType policy_type,
    Value::Type value_type)
    : policy_type_(policy_type),
      value_type_(value_type) {
}

TypeCheckingPolicyHandler::~TypeCheckingPolicyHandler() {
}

ConfigurationPolicyType TypeCheckingPolicyHandler::policy_type() const {
  return policy_type_;
}

bool TypeCheckingPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const Value* value = NULL;
  return CheckAndGetValue(policies, errors, &value);
}

bool TypeCheckingPolicyHandler::CheckAndGetValue(const PolicyMap& policies,
                                                 PolicyErrorMap* errors,
                                                 const Value** value) {
  *value = policies.Get(policy_type_);
  if (*value && !(*value)->IsType(value_type_)) {
    errors->AddError(policy_type_,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(value_type_));
    return false;
  }
  return true;
}

// SimplePolicyHandler implementation ------------------------------------------

SimplePolicyHandler::SimplePolicyHandler(
    ConfigurationPolicyType policy_type,
    Value::Type value_type,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_type, value_type),
      pref_path_(pref_path) {
}

SimplePolicyHandler::~SimplePolicyHandler() {
}

void SimplePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                              PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  if (value)
    prefs->SetValue(pref_path_, value->DeepCopy());
}


// SyncPolicyHandler implementation --------------------------------------------

SyncPolicyHandler::SyncPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicySyncDisabled,
                                Value::TYPE_BOOLEAN) {
}

SyncPolicyHandler::~SyncPolicyHandler() {
}

void SyncPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  bool disable_sync;
  if (value && value->GetAsBoolean(&disable_sync) && disable_sync)
    prefs->SetValue(prefs::kSyncManaged, value->DeepCopy());
}


// AutofillPolicyHandler implementation ----------------------------------------

AutofillPolicyHandler::AutofillPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyAutoFillEnabled,
                                Value::TYPE_BOOLEAN) {
}

AutofillPolicyHandler::~AutofillPolicyHandler() {
}

void AutofillPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  bool auto_fill_enabled;
  if (value && value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled) {
    prefs->SetValue(prefs::kAutofillEnabled,
                    Value::CreateBooleanValue(false));
  }
}


// DownloadDirPolicyHandler implementation -------------------------------------

DownloadDirPolicyHandler::DownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyDownloadDirectory,
                                Value::TYPE_STRING) {
}

DownloadDirPolicyHandler::~DownloadDirPolicyHandler() {
}

void DownloadDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  FilePath::StringType string_value;
  if (!value || !value->GetAsString(&string_value))
    return;

  FilePath::StringType expanded_value =
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
    : TypeCheckingPolicyHandler(kPolicyDiskCacheDir,
                                Value::TYPE_STRING) {
}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {
}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  FilePath::StringType string_value;
  if (value && value->GetAsString(&string_value)) {
    FilePath::StringType expanded_value =
        policy::path_parser::ExpandPathVariables(string_value);
    prefs->SetValue(prefs::kDiskCacheDir,
                    Value::CreateStringValue(expanded_value));
  }
}


// FileSelectionDialogsHandler implementation ----------------------------------

FileSelectionDialogsHandler::FileSelectionDialogsHandler()
    : TypeCheckingPolicyHandler(kPolicyAllowFileSelectionDialogs,
                                Value::TYPE_BOOLEAN) {
}

FileSelectionDialogsHandler::~FileSelectionDialogsHandler() {
}

void FileSelectionDialogsHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  bool allow_dialogs;
  const Value* value = policies.Get(policy_type());
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
  const Value* availability = policies.Get(kPolicyIncognitoModeAvailability);

  if (availability) {
    if (availability->GetAsInteger(&int_value)) {
      IncognitoModePrefs::Availability availability_enum_value;
      if (!IncognitoModePrefs::IntToAvailability(int_value,
                                                 &availability_enum_value)) {
        errors->AddError(kPolicyIncognitoModeAvailability,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::IntToString(int_value));
        return false;
      }
    } else {
      errors->AddError(kPolicyIncognitoModeAvailability,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }
  } else {
    const Value* deprecated_enabled = policies.Get(kPolicyIncognitoEnabled);
    if (deprecated_enabled &&
        !deprecated_enabled->IsType(Value::TYPE_BOOLEAN)) {
      errors->AddError(kPolicyIncognitoEnabled,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }
  }
  return true;
}

void IncognitoModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const Value* availability = policies.Get(kPolicyIncognitoModeAvailability);
  const Value* deprecated_enabled = policies.Get(kPolicyIncognitoEnabled);
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
    // If kPolicyIncognitoModeAvailability is not specified, check the obsolete
    // kPolicyIncognitoEnabled.
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
    : TypeCheckingPolicyHandler(kPolicyDefaultSearchProviderEncodings,
                                Value::TYPE_LIST) {
}

DefaultSearchEncodingsPolicyHandler::~DefaultSearchEncodingsPolicyHandler() {
}

void DefaultSearchEncodingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies, PrefValueMap* prefs) {
  // The DefaultSearchProviderEncodings policy has type list, but the related
  // preference has type string. Convert one into the other here, using
  // ';' as a separator.
  const Value* value = policies.Get(policy_type());
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
    ConfigurationPolicyType policy_type =
        kDefaultSearchPolicyMap[i].policy_type;
    if (policy_type == kPolicyDefaultSearchProviderEncodings) {
      handlers_.push_back(new DefaultSearchEncodingsPolicyHandler());
    } else {
      handlers_.push_back(
          new SimplePolicyHandler(policy_type,
                                  kDefaultSearchPolicyMap[i].value_type,
                                  kDefaultSearchPolicyMap[i].preference_path));
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
      ConfigurationPolicyType policy_type =
          kDefaultSearchPolicyMap[i].policy_type;
      if (policy_type != kPolicyDefaultSearchProviderEnabled &&
          HasDefaultSearchPolicy(policies, policy_type)) {
        errors->AddError(policy_type, IDS_POLICY_DEFAULT_SEARCH_DISABLED);
      }
    }
    return true;
  }

  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  if (!search_url && AnyDefaultSearchPoliciesSpecified(policies)) {
    errors->AddError(kPolicyDefaultSearchProviderSearchURL,
                     IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }

  if (search_url && !DefaultSearchURLIsValid(policies)) {
    errors->AddError(kPolicyDefaultSearchProviderSearchURL,
                     IDS_POLICY_INVALID_SEARCH_URL_ERROR);
    return false;
  }
  return true;
}

void DefaultSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  if (DefaultSearchProviderIsDisabled(policies)) {
    // If default search is disabled, the other fields are ignored.
    prefs->SetString(prefs::kDefaultSearchProviderName, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSearchURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderIconURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderEncodings, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderKeyword, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderInstantURL, std::string());
    return;
  }

  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  // The search URL is required.
  if (!search_url)
    return;

  // The other entries are optional.  Just make sure that they are all
  // specified via policy, so that the regular prefs aren't used.
  if (DefaultSearchURLIsValid(policies)) {
    std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
    for (handler = handlers_.begin() ; handler != handlers_.end(); ++handler)
      (*handler)->ApplyPolicySettings(policies, prefs);

    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderSuggestURL);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderIconURL);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderEncodings);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderKeyword);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderInstantURL);

    // For the name, default to the host if not specified.
    std::string name;
    if (!prefs->GetString(prefs::kDefaultSearchProviderName, &name) ||
        name.empty()) {
      std::string search_url_string;
      if (search_url->GetAsString(&search_url_string)) {
        prefs->SetString(prefs::kDefaultSearchProviderName,
                         GURL(search_url_string).host());
      }
    }

    // And clear the IDs since these are not specified via policy.
    prefs->SetString(prefs::kDefaultSearchProviderID, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID,
                     std::string());
  }
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
    ConfigurationPolicyType policy_type) {
  return policies.Get(policy_type) != NULL;
}

bool DefaultSearchPolicyHandler::AnyDefaultSearchPoliciesSpecified(
    const PolicyMap& policies) {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    if (policies.Get(kDefaultSearchPolicyMap[i].policy_type))
      return true;
  }
  return false;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderIsDisabled(
    const PolicyMap& policies) {
  const Value* provider_enabled =
      policies.Get(kPolicyDefaultSearchProviderEnabled);
  bool enabled = true;
  return provider_enabled &&
         provider_enabled->GetAsBoolean(&enabled) &&
         !enabled;
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies) {
  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  if (!search_url)
    return false;

  std::string search_url_string;
  if (search_url->GetAsString(&search_url_string)) {
    SearchTermsDataForValidation search_terms_data;
    const TemplateURLRef search_url_ref(search_url_string, 0, 0);
    // It must support replacement (which implies it is valid).
    return search_url_ref.SupportsReplacementUsingTermsData(search_terms_data);
  }
  return false;
}

void DefaultSearchPolicyHandler::EnsureStringPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  std::string value;
  if (!prefs->GetString(path, &value))
    prefs->SetString(path, value);
}


// ProxyPolicyHandler implementation -------------------------------------------

ProxyPolicyHandler::ProxyPolicyHandler() {
}

ProxyPolicyHandler::~ProxyPolicyHandler() {
}

bool ProxyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                             PolicyErrorMap* errors) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, kPolicyProxyBypassList);

  if ((server || pac_url || bypass_list) && !(mode || server_mode)) {
    errors->AddError(kPolicyProxyMode,
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

    if (!entry.pac_url_allowed && pac_url)
      errors->AddError(kPolicyProxyPacUrl, entry.error_message_id);
    if (!entry.bypass_list_allowed && bypass_list)
      errors->AddError(kPolicyProxyBypassList, entry.error_message_id);
    if (!entry.server_allowed && server)
      errors->AddError(kPolicyProxyServer, entry.error_message_id);

    if ((!entry.pac_url_allowed && pac_url) ||
        (!entry.bypass_list_allowed && bypass_list) ||
        (!entry.server_allowed && server)) {
      return false;
    }
  }

  if (!is_valid_mode) {
    errors->AddError(mode ? kPolicyProxyMode : kPolicyProxyServerMode,
                     IDS_POLICY_OUT_OF_RANGE_ERROR, mode_value);
    return false;
  }
  return true;
}

void ProxyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                             PrefValueMap* prefs) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, kPolicyProxyBypassList);

  ProxyPrefs::ProxyMode proxy_mode;
  if (mode) {
    std::string string_mode;
    CHECK(mode->GetAsString(&string_mode));
    CHECK(ProxyPrefs::StringToProxyMode(string_mode, &proxy_mode));
  } else if (server_mode) {
    int int_mode = 0;
    CHECK(server_mode->GetAsInteger(&int_mode));

    switch (int_mode) {
      case kPolicyNoProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        break;
      case kPolicyAutoDetectProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_AUTO_DETECT;
        break;
      case kPolicyManuallyConfiguredProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_FIXED_SERVERS;
        if (pac_url)
          proxy_mode = ProxyPrefs::MODE_PAC_SCRIPT;
        break;
      case kPolicyUseSystemProxyServerMode:
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
      if (pac_url->GetAsString(&pac_url_string)) {
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
    const PolicyMap& policies, ConfigurationPolicyType policy) {
  const Value* value = policies.Get(policy);
  std::string tmp;
  if (!value ||
      value->IsType(Value::TYPE_NULL) ||
      (value->IsType(Value::TYPE_STRING) &&
      value->GetAsString(&tmp) &&
      tmp.empty())) {
    return NULL;
  }
  return value;
}

bool ProxyPolicyHandler::CheckProxyModeAndServerMode(const PolicyMap& policies,
                                                     PolicyErrorMap* errors,
                                                     std::string* mode_value) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);

  // If there's a server mode, convert it into a mode.
  // When both are specified, the mode takes precedence.
  if (mode) {
    if (server_mode) {
      errors->AddError(kPolicyProxyServerMode,
                       IDS_POLICY_OVERRIDDEN,
                       GetPolicyName(kPolicyProxyMode));
    }
    if (!mode->GetAsString(mode_value)) {
      errors->AddError(kPolicyProxyMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }

    ProxyPrefs::ProxyMode mode;
    if (!ProxyPrefs::StringToProxyMode(*mode_value, &mode)) {
      errors->AddError(kPolicyProxyMode, IDS_POLICY_INVALID_PROXY_MODE_ERROR);
      return false;
    }

    if (mode == ProxyPrefs::MODE_PAC_SCRIPT && !pac_url) {
      errors->AddError(kPolicyProxyPacUrl, IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    } else if (mode == ProxyPrefs::MODE_FIXED_SERVERS && !server) {
      errors->AddError(kPolicyProxyServer, IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    }
  } else if (server_mode) {
    int server_mode_value;
    if (!server_mode->GetAsInteger(&server_mode_value)) {
      errors->AddError(kPolicyProxyServerMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }

    switch (server_mode_value) {
      case kPolicyNoProxyServerMode:
        *mode_value = ProxyPrefs::kDirectProxyModeName;
        break;
      case kPolicyAutoDetectProxyServerMode:
        *mode_value = ProxyPrefs::kAutoDetectProxyModeName;
        break;
      case kPolicyManuallyConfiguredProxyServerMode:
        if (server && pac_url) {
          int message_id = IDS_POLICY_PROXY_BOTH_SPECIFIED_ERROR;
          errors->AddError(kPolicyProxyServer, message_id);
          errors->AddError(kPolicyProxyPacUrl, message_id);
          return false;
        }
        if (!server && !pac_url) {
          int message_id = IDS_POLICY_PROXY_NEITHER_SPECIFIED_ERROR;
          errors->AddError(kPolicyProxyServer, message_id);
          errors->AddError(kPolicyProxyPacUrl, message_id);
          return false;
        }
        if (pac_url)
          *mode_value = ProxyPrefs::kPacScriptProxyModeName;
        else
          *mode_value = ProxyPrefs::kFixedServersProxyModeName;
        break;
      case kPolicyUseSystemProxyServerMode:
        *mode_value = ProxyPrefs::kSystemProxyModeName;
        break;
      default:
        errors->AddError(kPolicyProxyServerMode,
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
  const Value* javascript_enabled = policies.Get(kPolicyJavascriptEnabled);
  const Value* default_setting = policies.Get(kPolicyDefaultJavaScriptSetting);

  if (javascript_enabled && !javascript_enabled->IsType(Value::TYPE_BOOLEAN)) {
    errors->AddError(kPolicyJavascriptEnabled,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_BOOLEAN));
  }

  if (default_setting && !default_setting->IsType(Value::TYPE_INTEGER)) {
    errors->AddError(kPolicyDefaultJavaScriptSetting,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_INTEGER));
  }

  if (javascript_enabled && default_setting) {
    errors->AddError(kPolicyJavascriptEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     GetPolicyName(kPolicyDefaultJavaScriptSetting));
  }

  return true;
}

void JavascriptPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  int setting = CONTENT_SETTING_DEFAULT;
  const Value* default_setting = policies.Get(kPolicyDefaultJavaScriptSetting);

  if (default_setting) {
    default_setting->GetAsInteger(&setting);
  } else {
    const Value* javascript_enabled = policies.Get(kPolicyJavascriptEnabled);
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

}  // namespace policy

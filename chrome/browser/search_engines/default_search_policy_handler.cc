// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

// List of policy types to preference names, for policies affecting the default
// search provider.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyMap[] = {
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
  { key::kDefaultSearchProviderImageURL,
    prefs::kDefaultSearchProviderImageURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderNewTabURL,
    prefs::kDefaultSearchProviderNewTabURL,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderSearchURLPostParams,
    prefs::kDefaultSearchProviderSearchURLPostParams,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderSuggestURLPostParams,
    prefs::kDefaultSearchProviderSuggestURLPostParams,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderInstantURLPostParams,
    prefs::kDefaultSearchProviderInstantURLPostParams,
    Value::TYPE_STRING },
  { key::kDefaultSearchProviderImageURLPostParams,
    prefs::kDefaultSearchProviderImageURLPostParams,
    Value::TYPE_STRING },
};

// DefaultSearchEncodingsPolicyHandler implementation --------------------------

DefaultSearchEncodingsPolicyHandler::DefaultSearchEncodingsPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDefaultSearchProviderEncodings,
                                Value::TYPE_LIST) {}

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
      handlers_.push_back(new SimplePolicyHandler(
          policy_name,
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

    for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
             handlers_.begin();
         handler != handlers_.end(); ++handler) {
      const char* policy_name = (*handler)->policy_name();
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
    prefs->SetString(prefs::kDefaultSearchProviderNewTabURL, std::string());
    prefs->SetValue(prefs::kDefaultSearchProviderAlternateURLs,
                    new ListValue());
    prefs->SetString(
        prefs::kDefaultSearchProviderSearchTermsReplacementKey, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderImageURL, std::string());
    prefs->SetString(
        prefs::kDefaultSearchProviderSearchURLPostParams, std::string());
    prefs->SetString(
        prefs::kDefaultSearchProviderSuggestURLPostParams, std::string());
    prefs->SetString(
        prefs::kDefaultSearchProviderInstantURLPostParams, std::string());
    prefs->SetString(
        prefs::kDefaultSearchProviderImageURLPostParams, std::string());
  } else {
    // The search URL is required.  The other entries are optional.  Just make
    // sure that they are all specified via policy, so that the regular prefs
    // aren't used.
    const Value* dummy;
    std::string url;
    if (DefaultSearchURLIsValid(policies, &dummy, &url)) {

      for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
               handlers_.begin();
           handler != handlers_.end(); ++handler) {
        (*handler)->ApplyPolicySettings(policies, prefs);
      }

      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderSuggestURL);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderIconURL);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderEncodings);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderKeyword);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderInstantURL);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderNewTabURL);
      EnsureListPrefExists(prefs, prefs::kDefaultSearchProviderAlternateURLs);
      EnsureStringPrefExists(
          prefs,
          prefs::kDefaultSearchProviderSearchTermsReplacementKey);
      EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderImageURL);
      EnsureStringPrefExists(
          prefs,
          prefs::kDefaultSearchProviderSearchURLPostParams);
      EnsureStringPrefExists(
          prefs,
          prefs::kDefaultSearchProviderSuggestURLPostParams);
      EnsureStringPrefExists(
          prefs,
          prefs::kDefaultSearchProviderInstantURLPostParams);
      EnsureStringPrefExists(
          prefs,
          prefs::kDefaultSearchProviderImageURLPostParams);

      // For the name and keyword, default to the host if not specified.  If
      // there is no host (file: URLs?  Not sure), use "_" to guarantee that the
      // keyword is non-empty.
      std::string name, keyword;
      std::string host(GURL(url).host());
      if (host.empty())
        host = "_";
      if (!prefs->GetString(prefs::kDefaultSearchProviderName, &name) ||
          name.empty()) {
        prefs->SetString(prefs::kDefaultSearchProviderName, host);
      }
      if (!prefs->GetString(prefs::kDefaultSearchProviderKeyword, &keyword) ||
          keyword.empty()) {
        prefs->SetString(prefs::kDefaultSearchProviderKeyword, host);
      }

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
  for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
           handlers_.begin();
       handler != handlers_.end(); ++handler) {
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
  for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
           handlers_.begin();
       handler != handlers_.end(); ++handler) {
    if (policies.Get((*handler)->policy_name()))
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

}  // namespace policy

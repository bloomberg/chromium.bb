// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash_tables.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

const char PolicyUIHandler::kLevel[] = "level";
const char PolicyUIHandler::kName[] = "name";
const char PolicyUIHandler::kScope[] = "scope";
const char PolicyUIHandler::kSet[] = "set";
const char PolicyUIHandler::kStatus[] = "status";
const char PolicyUIHandler::kValue[] = "value";

namespace {

ChromeWebUIDataSource* CreatePolicyUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPolicyHost);

  // Localized strings.
  source->AddLocalizedString("policyTitle", IDS_POLICY_TITLE);
  source->AddLocalizedString("statusPaneTitle", IDS_POLICY_STATUS_TITLE);
  source->AddLocalizedString("fetchPoliciesText", IDS_POLICY_FETCH);
  source->AddLocalizedString("devicePoliciesBoxTitle",
                             IDS_POLICY_DEVICE_POLICIES);
  source->AddLocalizedString("userPoliciesBoxTitle", IDS_POLICY_USER_POLICIES);
  source->AddLocalizedString("enrollmentDomainText",
                             IDS_POLICY_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("clientIdText", IDS_POLICY_CLIENT_ID);
  source->AddLocalizedString("usernameText", IDS_POLICY_USERNAME);
  source->AddLocalizedString("lastFetchedText", IDS_POLICY_LAST_FETCHED);
  source->AddLocalizedString("fetchIntervalText", IDS_POLICY_FETCH_INTERVAL);
  source->AddLocalizedString("serverStatusText", IDS_POLICY_SERVER_STATUS);
  source->AddLocalizedString("showUnsentPoliciesText", IDS_POLICY_SHOW_UNSENT);
  source->AddLocalizedString("filterPoliciesText", IDS_POLICY_FILTER);
  source->AddLocalizedString("noPoliciesSet",IDS_POLICY_NO_POLICIES_SET);
  source->AddLocalizedString("appliesToTableHeader", IDS_POLICY_APPLIES_TO);
  source->AddLocalizedString("policyLevelTableHeader", IDS_POLICY_LEVEL);
  source->AddLocalizedString("policyNameTableHeader", IDS_POLICY_ENTRY_NAME);
  source->AddLocalizedString("policyValueTableHeader", IDS_POLICY_ENTRY_VALUE);
  source->AddLocalizedString("policyStatusTableHeader",
                             IDS_POLICY_ENTRY_STATUS);
  source->AddLocalizedString("showMoreText", IDS_POLICY_SHOW_MORE);
  source->AddLocalizedString("hideText", IDS_POLICY_HIDE);
  source->set_json_path("strings.js");

  // Add required resources.
  source->add_resource_path("policy.css", IDR_POLICY_CSS);
  source->add_resource_path("policy.js", IDR_POLICY_JS);
  source->set_default_resource(IDR_POLICY_HTML);

  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUIHandler
//
////////////////////////////////////////////////////////////////////////////////

PolicyUIHandler::PolicyUIHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  g_browser_process->policy_service()->AddObserver(
      policy::POLICY_DOMAIN_CHROME, "", this);
}

PolicyUIHandler::~PolicyUIHandler() {
  g_browser_process->policy_service()->RemoveObserver(
      policy::POLICY_DOMAIN_CHROME, "", this);
}

void PolicyUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestData",
      base::Bind(&PolicyUIHandler::HandleRequestData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchPolicy",
      base::Bind(&PolicyUIHandler::HandleFetchPolicy,
                 base::Unretained(this)));
}

void PolicyUIHandler::OnPolicyUpdated(policy::PolicyDomain domain,
                                      const std::string& component_id,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  DCHECK(domain == policy::POLICY_DOMAIN_CHROME);
  DCHECK(component_id.empty());
  SendDataToUI();
}

// static
scoped_ptr<base::ListValue> PolicyUIHandler::GetPolicyStatusList(
    const policy::PolicyMap& policies,
    bool* any_policies_set) {
  scoped_ptr<base::ListValue> result(new base::ListValue());
  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  policy::PolicyMap policy_map;
  policy_map.CopyFrom(policies);

  // Get a list of all the errors in the policy values.
  const policy::ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  policy::PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(policy_map, NULL, &errors);
  handler_list->PrepareForDisplaying(&policy_map);

  // Append each known policy and its status.
  *any_policies_set = false;
  std::vector<base::DictionaryValue*> unset_policies;
  base::hash_set<std::string> known_names;
  const policy::PolicyDefinitionList* policy_list =
      policy::GetChromePolicyDefinitionList();
  for (const policy::PolicyDefinitionList::Entry* policy = policy_list->begin;
       policy != policy_list->end; ++policy) {
    known_names.insert(policy->name);
    const policy::PolicyMap::Entry* entry = policy_map.Get(policy->name);
    string16 error_message = errors.GetErrors(policy->name);
    if (entry) {
      *any_policies_set = true;
      result->Append(GetPolicyDetails(policy, entry, error_message));
    } else {
      unset_policies.push_back(GetPolicyErrorDetails(policy->name, false));
    }
  }

  // Append unrecognized policy names.
  for (policy::PolicyMap::const_iterator it = policy_map.begin();
       it != policy_map.end(); ++it) {
    if (!ContainsKey(known_names, it->first))
      result->Append(GetPolicyErrorDetails(it->first, true));
  }

  // Append policies that aren't currently configured to the end.
  for (std::vector<base::DictionaryValue*>::const_iterator it =
           unset_policies.begin();
       it != unset_policies.end(); ++it) {
    result->Append(*it);
  }

  return result.Pass();
}

void PolicyUIHandler::HandleRequestData(const base::ListValue* args) {
  SendDataToUI();
}

void PolicyUIHandler::HandleFetchPolicy(const base::ListValue* args) {
  // Fetching policy can potentially take a while due to cloud policy fetches.
  // Use a WeakPtr to make sure the callback is invalidated if the tab is closed
  // before the fetching completes.
  g_browser_process->policy_service()->RefreshPolicies(
      base::Bind(&PolicyUIHandler::OnRefreshDone,
                 weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::OnRefreshDone() {
  web_ui()->CallJavascriptFunction("Policy.refreshDone");
}

void PolicyUIHandler::SendDataToUI() {
  policy::PolicyService* service = g_browser_process->policy_service();
  bool any_policies_set = false;
  base::ListValue* list =
      GetPolicyStatusList(
          service->GetPolicies(policy::POLICY_DOMAIN_CHROME, std::string()),
          &any_policies_set).release();
  base::DictionaryValue results;
  results.Set("policies", list);
  results.SetBoolean("anyPoliciesSet", any_policies_set);
  base::DictionaryValue* dict = GetStatusData();
  results.Set("status", dict);
  web_ui()->CallJavascriptFunction("Policy.returnData", results);
}

// static
string16 PolicyUIHandler::GetPolicyScopeString(policy::PolicyScope scope) {
  switch (scope) {
    case policy::POLICY_SCOPE_USER:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_USER);
    case policy::POLICY_SCOPE_MACHINE:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_MACHINE);
  }
  NOTREACHED();
  return string16();
}

// static
string16 PolicyUIHandler::GetPolicyLevelString(policy::PolicyLevel level) {
  switch (level) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_RECOMMENDED);
    case policy::POLICY_LEVEL_MANDATORY:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_MANDATORY);
  }
  NOTREACHED();
  return string16();
}

// static
base::DictionaryValue* PolicyUIHandler::GetPolicyDetails(
    const policy::PolicyDefinitionList::Entry* policy_definition,
    const policy::PolicyMap::Entry* policy_value,
    const string16& error_message) {
  base::DictionaryValue* details = new base::DictionaryValue();
  details->SetString(kName, ASCIIToUTF16(policy_definition->name));
  details->SetBoolean(kSet, true);
  details->SetString(kLevel, GetPolicyLevelString(policy_value->level));
  details->SetString(kScope, GetPolicyScopeString(policy_value->scope));
  details->Set(kValue, policy_value->value->DeepCopy());
  if (error_message.empty())
    details->SetString(kStatus, l10n_util::GetStringUTF16(IDS_OK));
  else
    details->SetString(kStatus, error_message);
  return details;
}

// static
base::DictionaryValue* PolicyUIHandler::GetPolicyErrorDetails(
    const std::string& policy_name,
    bool is_set) {
  base::DictionaryValue* details = new base::DictionaryValue();
  details->SetString(kName, ASCIIToUTF16(policy_name));
  details->SetBoolean(kSet, is_set);
  details->SetString(kLevel, "");
  details->SetString(kScope, "");
  details->SetString(kValue, "");
  if (is_set)
    details->SetString(kStatus, l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN));
  else
    details->SetString(kStatus, l10n_util::GetStringUTF16(IDS_POLICY_NOT_SET));
  return details;
}

// static
string16 PolicyUIHandler::CreateStatusMessageString(
    policy::CloudPolicySubsystem::ErrorDetails error_details) {
  static int error_to_string_id[] = {
    IDS_POLICY_STATUS_OK,
    IDS_POLICY_STATUS_NETWORK_ERROR,
    IDS_POLICY_STATUS_NETWORK_ERROR,  // this is also a network error.
    IDS_POLICY_STATUS_DMTOKEN_ERROR,
    IDS_POLICY_STATUS_LOCAL_ERROR,
    IDS_POLICY_STATUS_SIGNATURE_ERROR,
    IDS_POLICY_STATUS_SERIAL_ERROR,
  };
  DCHECK(static_cast<size_t>(error_details) < arraysize(error_to_string_id));
  return l10n_util::GetStringUTF16(error_to_string_id[error_details]);
}

base::DictionaryValue* PolicyUIHandler::GetStatusData() {
  base::DictionaryValue* results = new base::DictionaryValue();
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();

  policy::CloudPolicySubsystem* device_subsystem =
      connector->device_cloud_policy_subsystem();
  policy::CloudPolicySubsystem* user_subsystem =
      connector->user_cloud_policy_subsystem();

  bool device_status_available = false;
  bool user_status_available = false;

  if (device_subsystem && connector->IsEnterpriseManaged()) {
    device_status_available = true;

    results->SetString("deviceStatusMessage",
        CreateStatusMessageString(device_subsystem->error_details()));
    results->SetString("deviceLastFetchTime",
        GetLastFetchTime(device_subsystem));
    results->SetString("devicePolicyDomain",
        ASCIIToUTF16(connector->GetEnterpriseDomain()));
    results->SetString("deviceId",
        GetDeviceId(connector->GetDeviceCloudPolicyDataStore()));
    results->SetString("deviceFetchInterval",
        GetPolicyFetchInterval(prefs::kDevicePolicyRefreshRate));
  }

  if (user_subsystem &&
      user_subsystem->state() != policy::CloudPolicySubsystem::UNMANAGED) {
    user_status_available = true;

    results->SetString("userStatusMessage",
        CreateStatusMessageString(user_subsystem->error_details()));
    results->SetString("userLastFetchTime", GetLastFetchTime(user_subsystem));

#if defined(OS_CHROMEOS)
    const chromeos::User& user =
        chromeos::UserManager::Get()->GetLoggedInUser();
    results->SetString("user", ASCIIToUTF16(user.email()));
#else
    results->SetString("user", string16());
#endif

    results->SetString("userId",
        GetDeviceId(connector->GetUserCloudPolicyDataStore()));
    results->SetString("userFetchInterval",
        GetPolicyFetchInterval(prefs::kUserPolicyRefreshRate));
  }

  results->SetBoolean("displayDeviceStatus", device_status_available);
  results->SetBoolean("displayUserStatus", user_status_available);
  results->SetBoolean("displayStatusSection",
                      user_status_available || device_status_available);
  return results;
}

string16 PolicyUIHandler::GetLastFetchTime(
    policy::CloudPolicySubsystem* subsystem) {
  base::Time last_refresh_time =
      subsystem->GetCloudPolicyCacheBase()->last_policy_refresh_time();
  if (last_refresh_time.is_null())
    return l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  base::Time now = base::Time::NowFromSystemTime();
  return TimeFormat::TimeElapsed(now - last_refresh_time);
}

string16 PolicyUIHandler::GetDeviceId(
    const policy::CloudPolicyDataStore* data_store) {
  return data_store ? ASCIIToUTF16(data_store->device_id()) : string16();
}

string16 PolicyUIHandler::GetPolicyFetchInterval(const char* refresh_pref) {
  PrefService* prefs = g_browser_process->local_state();
  return TimeFormat::TimeRemainingShort(
      base::TimeDelta::FromMilliseconds(prefs->GetInteger(refresh_pref)));
}

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUI
//
////////////////////////////////////////////////////////////////////////////////

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PolicyUIHandler);

  // Set up the chrome://policy/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreatePolicyUIHTMLSource());
}

PolicyUI::~PolicyUI() {
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

ChromeWebUIDataSource* CreatePolicyUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPolicyHost);

  // Localized strings.
  source->AddLocalizedString("policyTitle", IDS_POLICY_TITLE);
  source->AddLocalizedString("statusPaneTitle", IDS_POLICY_STATUS_TITLE);
  source->AddLocalizedString("fetchPoliciesText", IDS_POLICY_FETCH);
  source->AddLocalizedString("devicePoliciesBoxTitle",
                             IDS_POLICY_DEVICE_POLICIES);
  source->AddLocalizedString("userPoliciesBoxTitle",
                             IDS_POLICY_USER_POLICIES);
  source->AddLocalizedString("enrollmentDomainText",
                             IDS_POLICY_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("clientIdText", IDS_POLICY_CLIENT_ID);
  source->AddLocalizedString("usernameText", IDS_POLICY_USERNAME);
  source->AddLocalizedString("lastFetchedText", IDS_POLICY_LAST_FETCHED);
  source->AddLocalizedString("fetchIntervalText", IDS_POLICY_FETCH_INTERVAL);
  source->AddLocalizedString("serverStatusText", IDS_POLICY_SERVER_STATUS);
  source->AddLocalizedString("showUnsentPoliciesText",
                             IDS_POLICY_SHOW_UNSENT);
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

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUIHandler
//
////////////////////////////////////////////////////////////////////////////////

PolicyUIHandler::PolicyUIHandler() {
  policy::ConfigurationPolicyReader* managed_platform =
      policy::ConfigurationPolicyReader::CreateManagedPlatformPolicyReader();
  policy::ConfigurationPolicyReader* managed_cloud =
      policy::ConfigurationPolicyReader::CreateManagedCloudPolicyReader();
  policy::ConfigurationPolicyReader* recommended_platform =
      policy::ConfigurationPolicyReader::
          CreateRecommendedPlatformPolicyReader();
  policy::ConfigurationPolicyReader* recommended_cloud =
      policy::ConfigurationPolicyReader::CreateRecommendedCloudPolicyReader();
  policy_status_.reset(new policy::PolicyStatus(managed_platform,
                                                managed_cloud,
                                                recommended_platform,
                                                recommended_cloud));
  policy_status_->AddObserver(this);
}

PolicyUIHandler::~PolicyUIHandler() {
  policy_status_->RemoveObserver(this);
}

void PolicyUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "requestData",
      base::Bind(&PolicyUIHandler::HandleRequestData,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "fetchPolicy",
      base::Bind(&PolicyUIHandler::HandleFetchPolicy,
                 base::Unretained(this)));
}

void PolicyUIHandler::OnPolicyValuesChanged() {
  SendDataToUI(true);
}

void PolicyUIHandler::HandleRequestData(const ListValue* args) {
  SendDataToUI(false);
}

void PolicyUIHandler::HandleFetchPolicy(const ListValue* args) {
  g_browser_process->browser_policy_connector()->RefreshPolicies();
}

void PolicyUIHandler::SendDataToUI(bool is_policy_update) {
  DictionaryValue results;
  bool any_policies_set;
  ListValue* list = policy_status_->GetPolicyStatusList(&any_policies_set);
  results.Set("policies", list);
  results.SetBoolean("anyPoliciesSet", any_policies_set);
  DictionaryValue* dict = GetStatusData();
  results.Set("status", dict);
  results.SetBoolean("isPolicyUpdate", is_policy_update);

  web_ui_->CallJavascriptFunction("Policy.returnData", results);
}

DictionaryValue* PolicyUIHandler::GetStatusData() {
  DictionaryValue* results = new DictionaryValue();
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
    const chromeos::User& user = chromeos::UserManager::Get()->logged_in_user();
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

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUI
//
////////////////////////////////////////////////////////////////////////////////

PolicyUI::PolicyUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new PolicyUIHandler)->Attach(this));

  // Set up the chrome://policy/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(CreatePolicyUIHTMLSource());
}

PolicyUI::~PolicyUI() {
}

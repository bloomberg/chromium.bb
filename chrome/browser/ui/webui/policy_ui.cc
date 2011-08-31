// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include "chrome/browser/policy/policy_status_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

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
      ConfigurationPolicyReader::CreateManagedPlatformPolicyReader();
  policy::ConfigurationPolicyReader* managed_cloud =
      ConfigurationPolicyReader::CreateManagedCloudPolicyReader();
  policy::ConfigurationPolicyReader* recommended_platform =
      ConfigurationPolicyReader::CreateRecommendedPlatformPolicyReader();
  policy::ConfigurationPolicyReader* recommended_cloud =
      ConfigurationPolicyReader::CreateRecommendedCloudPolicyReader();
  policy_status_.reset(new policy::PolicyStatus(managed_platform,
                                                managed_cloud,
                                                recommended_platform,
                                                recommended_cloud));
}

PolicyUIHandler::~PolicyUIHandler() {
}

WebUIMessageHandler* PolicyUIHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void PolicyUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestPolicyData",
      NewCallback(this, &PolicyUIHandler::HandleRequestPolicyData));
}

void PolicyUIHandler::HandleRequestPolicyData(const ListValue* args) {
  bool any_policies_sent;
  ListValue* list = policy_status_->GetPolicyStatusList(&any_policies_sent);
  DictionaryValue results;
  results.Set("policies", list);
  results.SetBoolean("anyPoliciesSet", any_policies_sent);
  web_ui_->CallJavascriptFunction("Policy.returnPolicyData", results);
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

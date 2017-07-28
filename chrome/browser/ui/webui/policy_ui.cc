// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreatePolicyUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);
  source->AddLocalizedString("filterPlaceholder",
                             IDS_POLICY_FILTER_PLACEHOLDER);
  source->AddLocalizedString("reloadPolicies", IDS_POLICY_RELOAD_POLICIES);
  source->AddLocalizedString("chromeForWork", IDS_POLICY_CHROME_FOR_WORK);
  source->AddLocalizedString("status", IDS_POLICY_STATUS);
  source->AddLocalizedString("statusDevice", IDS_POLICY_STATUS_DEVICE);
  source->AddLocalizedString("statusUser", IDS_POLICY_STATUS_USER);
  source->AddLocalizedString("labelEnterpriseEnrollmentDomain",
                             IDS_POLICY_LABEL_ENTERPRISE_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("labelEnterpriseDisplayDomain",
                             IDS_POLICY_LABEL_ENTERPRISE_DISPLAY_DOMAIN);
  source->AddLocalizedString("labelUsername", IDS_POLICY_LABEL_USERNAME);
  source->AddLocalizedString("labelClientId", IDS_POLICY_LABEL_CLIENT_ID);
  source->AddLocalizedString("labelAssetId", IDS_POLICY_LABEL_ASSET_ID);
  source->AddLocalizedString("labelLocation", IDS_POLICY_LABEL_LOCATION);
  source->AddLocalizedString("labelDirectoryApiId",
                             IDS_POLICY_LABEL_DIRECTORY_API_ID);
  source->AddLocalizedString("labelTimeSinceLastRefresh",
                             IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH);
  source->AddLocalizedString("labelRefreshInterval",
                             IDS_POLICY_LABEL_REFRESH_INTERVAL);
  source->AddLocalizedString("labelStatus", IDS_POLICY_LABEL_STATUS);
  source->AddLocalizedString("showUnset", IDS_POLICY_SHOW_UNSET);
  source->AddLocalizedString("noPoliciesSet", IDS_POLICY_NO_POLICIES_SET);
  source->AddLocalizedString("showExpandedValue",
                             IDS_POLICY_SHOW_EXPANDED_VALUE);
  source->AddLocalizedString("hideExpandedValue",
                             IDS_POLICY_HIDE_EXPANDED_VALUE);
  // Add required resources.
  source->AddResourcePath("policy.css", IDR_POLICY_CSS);
  source->AddResourcePath("policy.js", IDR_POLICY_JS);
  source->SetDefaultResource(IDR_POLICY_HTML);
  return source;
}

}  // namespace

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<PolicyUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePolicyUIHtmlSource());
}

PolicyUI::~PolicyUI() {
}

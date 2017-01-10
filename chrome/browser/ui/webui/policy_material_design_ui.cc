// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_material_design_ui.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/policy_resources.h"
#include "chrome/grit/policy_resources_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/risk_tag.h"
#include "components/strings/grit/components_strings.h"

namespace {

// Strings that map from policy::RiskTag enum to i18n string keys and their IDs.
// Their order has to follow the order of the policy::RiskTag enum.
const PolicyStringMap kPolicyRiskTags[policy::RISK_TAG_COUNT] = {
  {"fullAdminAccess", IDS_POLICY_RISK_TAG_FULL_ADMIN_ACCESS},
  {"systemSecurity", IDS_POLICY_RISK_TAG_SYSTEM_SECURITY},
  {"websiteSharing", IDS_POLICY_RISK_TAG_WEBSITE_SHARING},
  {"adminSharing", IDS_POLICY_RISK_TAG_ADMIN_SHARING},
  {"filtering", IDS_POLICY_RISK_TAG_FILTERING},
  {"localDataAccess", IDS_POLICY_RISK_TAG_LOCAL_DATA_ACCESS},
  {"googleSharing", IDS_POLICY_RISK_TAG_GOOGLE_SHARING},
};

content::WebUIDataSource* CreatePolicyMaterialDesignUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);
  PolicyUIHandler::AddLocalizedPolicyStrings(
      source, kPolicyRiskTags, static_cast<size_t>(policy::RISK_TAG_COUNT));
  for (size_t i = 0; i < kPolicyResourcesSize; ++i) {
    source->AddResourcePath(kPolicyResources[i].name,
                            kPolicyResources[i].value);
  }
  source->SetDefaultResource(IDR_MD_POLICY_HTML);
  return source;
}

} // namespace

// The JavaScript message handler for the chrome://md-policy page.
class PolicyMaterialDesignUIHandler : public PolicyUIHandler {
 public:
  PolicyMaterialDesignUIHandler();
  ~PolicyMaterialDesignUIHandler() override;

 protected:
  // PolicyUIHandler:
  void AddPolicyName(const std::string& name,
                     base::DictionaryValue* names) const override;
  void SendPolicyNames() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyMaterialDesignUIHandler);
};

PolicyMaterialDesignUIHandler::PolicyMaterialDesignUIHandler() {
}

PolicyMaterialDesignUIHandler::~PolicyMaterialDesignUIHandler() {
}

void PolicyMaterialDesignUIHandler::AddPolicyName(
    const std::string& name, base::DictionaryValue* names) const {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  const policy::RiskTag* tags = policy::GetChromePolicyDetails(name)->risk_tags;
  for (size_t i = 0; i < policy::kMaxRiskTagCount; ++i) {
    if (tags[i] != policy::RISK_TAG_NONE)
      list->AppendString(kPolicyRiskTags[tags[i]].key);
  }
  names->Set(name, std::move(list));
}

void PolicyMaterialDesignUIHandler::SendPolicyNames() const {
  base::ListValue tags;
  for (size_t tag = 0; tag < policy::RISK_TAG_COUNT; ++tag)
    tags.AppendString(kPolicyRiskTags[tag].key);

  web_ui()->CallJavascriptFunctionUnsafe("policy.Page.setPolicyGroups", tags);
  PolicyUIHandler::SendPolicyNames();
}

PolicyMaterialDesignUI::PolicyMaterialDesignUI(content::WebUI* web_ui) :
    WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<PolicyMaterialDesignUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePolicyMaterialDesignUIHtmlSource());
}

PolicyMaterialDesignUI::~PolicyMaterialDesignUI() {
}

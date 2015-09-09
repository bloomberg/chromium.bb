// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_policy_connector.h"

#include "base/bind.h"
#include "components/policy/core/browser/android/android_combined_policy_provider.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"

namespace android_webview {

namespace {

// Callback only used in ChromeOS. No-op here.
void PopulatePolicyHandlerParameters(
    policy::PolicyHandlerParameters* parameters) {}

// Used to check if a policy is deprecated. Currently bypasses that check.
const policy::PolicyDetails* GetChromePolicyDetails(const std::string& policy) {
  return nullptr;
}

// Factory for the handlers that will be responsible for converting the policies
// to the associated preferences.
scoped_ptr<policy::ConfigurationPolicyHandlerList> BuildHandlerList(
    const policy::Schema& chrome_schema) {
  scoped_ptr<policy::ConfigurationPolicyHandlerList> handlers(
      new policy::ConfigurationPolicyHandlerList(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&GetChromePolicyDetails)));

  handlers->AddHandler(make_scoped_ptr(new policy::SimplePolicyHandler(
      policy::key::kURLWhitelist, policy::policy_prefs::kUrlWhitelist,
      base::Value::TYPE_LIST)));

  handlers->AddHandler(
      make_scoped_ptr(new policy::URLBlacklistPolicyHandler()));
  return handlers.Pass();
}

}  // namespace

AwBrowserPolicyConnector::AwBrowserPolicyConnector()
   : BrowserPolicyConnectorBase(base::Bind(&BuildHandlerList)) {
 SetPlatformPolicyProvider(make_scoped_ptr(
     new policy::android::AndroidCombinedPolicyProvider(GetSchemaRegistry())));
 InitPolicyProviders();
}

AwBrowserPolicyConnector::~AwBrowserPolicyConnector() {}

}  // namespace android_webview

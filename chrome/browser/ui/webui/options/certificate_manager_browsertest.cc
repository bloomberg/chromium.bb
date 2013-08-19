// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "crypto/nss_util.h"
#endif

using testing::AnyNumber;
using testing::Return;
using testing::_;

class CertificateManagerBrowserTest : public options::OptionsUIBrowserTest {
 public:
  CertificateManagerBrowserTest() {}
  virtual ~CertificateManagerBrowserTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Setup the policy provider for injecting certs through ONC policy.
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider_, RegisterPolicyDomain(_)).Times(AnyNumber());
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
#if defined(OS_CHROMEOS)
    Profile* profile = browser()->profile();
    policy::ProfilePolicyConnector* connector =
        policy::ProfilePolicyConnectorFactory::GetForProfile(profile);

    // Enable web trust certs from policy.
    g_browser_process->browser_policy_connector()->
        network_configuration_updater()->SetUserPolicyService(
            true, NULL /* no user */, connector->policy_service());
#endif
    content::RunAllPendingInMessageLoop();
  }

#if defined(OS_CHROMEOS)
  void LoadONCPolicy(const std::string& filename) {
    const std::string& user_policy_blob =
        chromeos::onc::test_utils::ReadTestData(filename);
    policy::PolicyMap policy;
    policy.Set(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER,
               Value::CreateStringValue(user_policy_blob),
               NULL);
    provider_.UpdateChromePolicy(policy);
    content::RunAllPendingInMessageLoop();
  }
#endif

  void ClickElement(const std::string& selector) {
    EXPECT_TRUE(content::ExecuteScriptInFrame(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "//div[@id='settings']/iframe",
        "document.querySelector(\"" + selector + "\").click()"));
  }

  bool HasElement(const std::string& selector) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptInFrameAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "//div[@id='settings']/iframe",
        "window.domAutomationController.send("
        "    !!document.querySelector('" + selector + "'));",
        &result));
    return result;
  }

  policy::MockConfigurationPolicyProvider provider_;
#if defined(OS_CHROMEOS)
  crypto::ScopedTestNSSDB test_nssdb_;
#endif
};

#if defined(OS_CHROMEOS)
// Ensure policy-installed certificates without web trust do not display
// the managed setting indicator (only on Chrome OS).
IN_PROC_BROWSER_TEST_F(CertificateManagerBrowserTest,
                       PolicyCertificateWithoutWebTrustHasNoIndicator) {
  LoadONCPolicy("certificate-authority.onc");
  NavigateToSettings();
  ClickElement("#certificatesManageButton");
  ClickElement("#ca-certs-nav-tab");
  EXPECT_FALSE(HasElement(".cert-policy"));
}
#endif

#if defined(OS_CHROMEOS)
// Ensure policy-installed certificates with web trust display the
// managed setting indicator (only on Chrome OS).
IN_PROC_BROWSER_TEST_F(CertificateManagerBrowserTest,
                       PolicyCertificateWithWebTrustHasIndicator) {
  LoadONCPolicy("certificate-web-authority.onc");
  NavigateToSettings();
  ClickElement("#certificatesManageButton");
  ClickElement("#ca-certs-nav-tab");
  EXPECT_TRUE(HasElement(".cert-policy"));
}
#endif

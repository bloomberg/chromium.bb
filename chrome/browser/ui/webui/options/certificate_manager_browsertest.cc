// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "crypto/nss_util.h"
#endif

using testing::Return;
using testing::_;

class CertificateManagerBrowserTest : public options::OptionsUIBrowserTest {
 public:
  CertificateManagerBrowserTest() {}
  virtual ~CertificateManagerBrowserTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
#if defined(OS_CHROMEOS)
    policy::UserNetworkConfigurationUpdater::
        SetSkipCertificateImporterCreationForTest(true);
#endif
    options::OptionsUIBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
#if defined(OS_CHROMEOS)
    policy::UserNetworkConfigurationUpdater::
        SetSkipCertificateImporterCreationForTest(false);
#endif
    options::OptionsUIBrowserTest::TearDown();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
#if defined(OS_CHROMEOS)
    device_policy_test_helper_.MarkAsEnterpriseOwned();
#endif
    // Setup the policy provider for injecting certs through ONC policy.
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnIOThread() {
#if defined(OS_CHROMEOS)
    test_nssdb_.reset(new crypto::ScopedTestNSSDB());
#endif
  }

  void TearDownOnIOThread() {
#if defined(OS_CHROMEOS)
    test_nssdb_.reset();
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CertificateManagerBrowserTest::SetUpOnIOThread, this));

    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    content::RunAllPendingInMessageLoop();

#if defined(OS_CHROMEOS)
    // UserNetworkConfigurationUpdater's onc::CertificateImporter is usually
    // passed the NSSCertDatabase fetched during testing profile
    // constrution. Unfortunately, test database gets setup after that, so we
    // would end up with |PK11_GetInternalKeySlot|. The cause of this is in
    // |crypto::InitializeNSSForChromeOSUser|, which does not open new
    // database slot for primary user, but it just uses the singleton one (which
    // is not set in tests before |test_nssdb_| is created). To handle this,
    // creating certificate importer during the UserNetworkConfiguirationUpdater
    // service creation is set to be skipped (see |SetUp|), and cert importer
    // is set up here.
    // Note that creating |test_nssdb_| sooner (in SetUp) would break thread
    // restrictions, which require it to be used on IO thread only.
    // TODO(tbarzic): Update InitializeNSSForChromeOSUser not to special case
    // the primary user.
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::Bind(
            &CertificateManagerBrowserTest::UpdateNetworkConfigurationUpdater,
            base::Unretained(this)));

    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    content::RunAllPendingInMessageLoop();
#endif
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CertificateManagerBrowserTest::TearDownOnIOThread, this));
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

#if defined(OS_CHROMEOS)
  void UpdateNetworkConfigurationUpdater(net::NSSCertDatabase* database) {
    policy::UserNetworkConfigurationUpdaterFactory::GetForProfile(
        browser()->profile())->SetCertificateImporterForTest(
            scoped_ptr<chromeos::onc::CertificateImporter>(
                new chromeos::onc::CertificateImporterImpl(database)));
  }

  void LoadONCPolicy(const std::string& filename) {
    const std::string& user_policy_blob =
        chromeos::onc::test_utils::ReadTestData(filename);
    policy::PolicyMap policy;
    policy.Set(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER,
               base::Value::CreateStringValue(user_policy_blob),
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
  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  scoped_ptr<crypto::ScopedTestNSSDB> test_nssdb_;
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

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "policy/policy_constants.h"

namespace {

const char kDeviceId[] = "device_id";
const base::FilePath::CharType kTestExtensionDir[] =
    FILE_PATH_LITERAL("extensions/api_test/enterprise_device_attributes");
const base::FilePath::CharType kUpdateManifestFileName[] =
    FILE_PATH_LITERAL("update_manifest.xml");

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kTestExtensionID[] = "nbiliclbejdndfpchgkbmfoppjplbdok";

}  // namespace

namespace extensions {

class EnterpriseDeviceAttributesTest : public ExtensionApiTest {
 public:
  explicit EnterpriseDeviceAttributesTest(const std::string& domain)
      : fake_session_manager_client_(new chromeos::FakeSessionManagerClient),
        test_domain_(domain) {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        make_scoped_ptr(fake_session_manager_client_));
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // Set up fake install attributes.
    scoped_ptr<policy::StubEnterpriseInstallAttributes> attributes(
        new policy::StubEnterpriseInstallAttributes());

    attributes->SetDomain(test_domain_);
    attributes->SetRegistrationUser(
        chromeos::login::StubAccountId().GetUserEmail());
    policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
        attributes.release());

    test_helper_.InstallOwnerKey();
    // Init the device policy.
    policy::DevicePolicyBuilder* device_policy = test_helper_.device_policy();
    device_policy->SetDefaultSigningKey();
    device_policy->policy_data().set_directory_api_id(kDeviceId);
    device_policy->Build();

    fake_session_manager_client_->set_device_policy(device_policy->GetBlob());
    fake_session_manager_client_->OnPropertyChangeComplete(true);

    // Init the user policy provider.
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // Enable the URLRequestMock, which is required for force-installing the
    // test extension through policy.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(chrome_browser_net::SetUrlRequestMocksEnabled, true));

    SetPolicy();
  }

 private:
  void SetPolicy() {
    // Extensions that are force-installed come from an update URL, which
    // defaults to the webstore. Use a mock URL for this test with an update
    // manifest that includes the crx file of the test extension.
    base::FilePath update_manifest_path =
        base::FilePath(kTestExtensionDir).Append(kUpdateManifestFileName);
    GURL update_manifest_url(net::URLRequestMockHTTPJob::GetMockUrl(
        update_manifest_path.MaybeAsASCII()));

    scoped_ptr<base::ListValue> forcelist(new base::ListValue);
    forcelist->AppendString(base::StringPrintf(
        "%s;%s", kTestExtensionID, update_manifest_url.spec().c_str()));

    policy::PolicyMap policy;
    policy.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, forcelist.release(), nullptr);

    // Set the policy and wait until the extension is installed.
    extensions::TestExtensionRegistryObserver observer(
        ExtensionRegistry::Get(profile()));
    policy_provider_.UpdateChromePolicy(policy);
    observer.WaitForExtensionLoaded();
  }

  chromeos::FakeSessionManagerClient* const fake_session_manager_client_;
  policy::MockConfigurationPolicyProvider policy_provider_;
  policy::DevicePolicyCrosTestHelper test_helper_;
  const std::string test_domain_;
};

// Creates affiliated user before browser initializes.
class EnterpriseDeviceAttributesAffiliatedTest
    : public EnterpriseDeviceAttributesTest {
 public:
  EnterpriseDeviceAttributesAffiliatedTest()
      : EnterpriseDeviceAttributesTest("gmail.com") {}
};

// Creates non-affiliated user before browser init.
class EnterpriseDeviceAttributesNonAffiliatedTest
    : public EnterpriseDeviceAttributesTest {
 public:
  EnterpriseDeviceAttributesNonAffiliatedTest()
      : EnterpriseDeviceAttributesTest("example.com") {}
};

// Tests the case of an affiliated user and pre-installed extension. Fetches
// the valid cloud directory device id.
IN_PROC_BROWSER_TEST_F(EnterpriseDeviceAttributesAffiliatedTest, Success) {
  // Pass the expected value (device_id) to test.
  ASSERT_TRUE(RunExtensionSubtest(
      "", base::StringPrintf("chrome-extension://%s/basic.html?%s",
                             kTestExtensionID, kDeviceId)))
      << message_;
}

// Test the case of non-affiliated user and pre-installed by policy extension.
// Extension API is available, but fetches the empty string.
IN_PROC_BROWSER_TEST_F(EnterpriseDeviceAttributesNonAffiliatedTest,
                       EmptyString) {
  // Pass the expected value (empty string) to test.
  ASSERT_TRUE(RunExtensionSubtest(
      "", base::StringPrintf("chrome-extension://%s/basic.html?%s",
                             kTestExtensionID, "")))
      << message_;
}

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.deviceAttributes permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.deviceAttributes namespace.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    EnterpriseDeviceAttributesIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionSubtest("enterprise_device_attributes",
                                  "api_not_available.html",
                                  kFlagIgnoreManifestWarnings));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("enterprise_device_attributes");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const extensions::Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.deviceAttributes' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

}  //  namespace extensions

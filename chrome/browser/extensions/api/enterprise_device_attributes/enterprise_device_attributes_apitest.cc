// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "extensions/browser/api_test_utils.h"

namespace {
const std::string kDeviceId = "device_id";
}  // namespace

namespace extensions {

class EnterpriseDeviceAttributesTest : public ExtensionApiTest {
 public:
  explicit EnterpriseDeviceAttributesTest(const std::string& domain)
      : fake_session_manager_client_(new chromeos::FakeSessionManagerClient),
        test_domain_(domain),
        get_device_id_function_(
            new EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction()) {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        make_scoped_ptr(fake_session_manager_client_));
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // Set up fake install attributes.
    scoped_ptr<policy::StubEnterpriseInstallAttributes> attributes(
        new policy::StubEnterpriseInstallAttributes());

    attributes->SetDomain(test_domain_);
    attributes->SetRegistrationUser(chromeos::login::kStubUser);
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
  }

  void RunAffiliationTest(const std::string& expect_value) {
    scoped_ptr<base::Value> result(
        api_test_utils::RunFunctionAndReturnSingleResult(
            get_device_id_function_.get(), "[]", browser()->profile()));

    std::string value;
    EXPECT_TRUE(result->GetAsString(&value));

    EXPECT_EQ(expect_value, value);
  }

 private:
  chromeos::FakeSessionManagerClient* const fake_session_manager_client_;
  policy::DevicePolicyCrosTestHelper test_helper_;
  const std::string test_domain_;
  const scoped_refptr<EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction>
      get_device_id_function_;
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

// Tests the case when the user is affiliated.
// Expected result: successful device id fetch.
IN_PROC_BROWSER_TEST_F(EnterpriseDeviceAttributesAffiliatedTest, Success) {
  RunAffiliationTest(kDeviceId);
}

// Tests the case when the user is not affiliated Expected result: fetch empty
// string as a device id..
IN_PROC_BROWSER_TEST_F(EnterpriseDeviceAttributesNonAffiliatedTest,
                       EmptyString) {
  RunAffiliationTest("");
}

}  //  namespace extensions

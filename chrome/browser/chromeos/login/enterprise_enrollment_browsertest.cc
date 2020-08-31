// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_impl.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace chromeos {

namespace {

constexpr char kEnrollmentUI[] = "enterprise-enrollment";
constexpr char kAdDialog[] = "oauth-enroll-ad-join-ui";
constexpr char kBackButton[] = "oobe-signin-back-button";

constexpr char kAdUnlockConfigurationStep[] = "unlockStep";
constexpr char kAdUnlockPasswordInput[] = "unlockPasswordInput";
constexpr char kAdUnlockButton[] = "unlockButton";
constexpr char kAdErrorButton[] = "ad-join-error-retry-button";
constexpr char kSkipButton[] = "skipButton";

constexpr char kAdCredentialsStep[] = "credsStep";
constexpr char kAdJoinConfigurationForm[] = "joinConfig";
constexpr char kAdBackToUnlockButton[] = "backToUnlockButton";
constexpr char kAdMachineNameInput[] = "machineNameInput";
constexpr char kAdUsernameInput[] = "userInput";
constexpr char kAdPasswordInput[] = "passwordInput";
constexpr char kAdConfigurationSelect[] = "joinConfigSelect";
constexpr char kNextButton[] = "nextButton";
constexpr char kWebview[] = "oauth-enroll-auth-view";
constexpr char kPartitionAttribute[] = ".partition";

constexpr char kAdEncryptionTypesSelect[] = "encryptionList";
constexpr char kAdMachineOrgUnitInput[] = "orgUnitInput";
constexpr char kAdMoreOptionsButton[] = "moreOptionsBtn";
constexpr char kAdMoreOptionsSaveButton[] = "moreOptionsSave";

constexpr char kAdUserDomain[] = "user.domain.com";
constexpr char kAdMachineDomain[] = "machine.domain.com";
constexpr char kAdMachineDomainDN[] =
    "OU=leaf,OU=root,DC=machine,DC=domain,DC=com";
constexpr const char* kAdOrganizationlUnit[] = {"leaf", "root"};
constexpr char kAdTestUser[] = "test_user@user.domain.com";
constexpr char kDMToken[] = "dm_token";
constexpr char kAdDomainJoinEncryptedConfig[] =
    "W/x3ToZtYrHTzD21dlx2MrMhs2bDFTNmvew/toQhO+RdBV8XmQfJqVMaRtIO+Uji6zBueUyxcl"
    "MtiFJnimvYh0DUFQ5PJ3PY49BPACPnrGws51or1pEZJkXiKOEapRwNfqHz5tOnvFS1VqSvcv6Z"
    "JQqFQHKfvodGiEZv52+iViQTCSup8VJWCtfJxy/LxqHly/4xaUDNn8Sbbv8V/j8HUxc7/rwmnm"
    "R5B6qxIYDfYOpZWQXnVunlB2bBkcCEgXdS9YN+opsftlkNPsVrcdHUWwCmqxxAsuVZaTfxu+7C"
    "ZhSG72VH3BtQUsyGoh9evSIhAcid1CGbSx16sJVZyhZVXMF9D80AEl6aWDyxh43iJy0AgLpfkP"
    "mfkpZ3+iv0EJocFUhFINrq0fble+wE8KsOtlUBne4jFA/fifOrRBoIdXdLLz3+FbL4A7zY9qbd"
    "PbDw5J5W3nnaJWhTd5R765LbPp7wNAzdPh4a++E0dUUSVXO2K5HkAopV9RkeDea2kaxOLi1ioj"
    "H8fxubSHp4e8ZYSAX4N9JkJWiDurp8yEpUno2aw2Y7HafkMs0GMnO0sdkJfLZrnDq9wkZh7bMD"
    "6sp5tiOqVbTG6QH1BdlJBryTAjlrMFL6y7zFvfMZSJhbI6VwQyskGX/TOYEnsXuWEpRBxtDVV/"
    "QLUWM0orFELZPoPdeuH3dACsEv4mMBo8hWlKu/S3SHXt2hrvI1PXDO10AOHy8CPNPs7p/LeuJq"
    "XHRYOKsuNZnYbFJR1r+rZhkvYFpn6dHOLbe7RScqkq9cUYVvxK84COIdbEay9w1Son4sFJZszi"
    "Ve+uc/oFWcVp6GZPzvWSfjrTXYqIFDw/WsC8mYMgqOvTZCKj6M3pUyvc7bT3hIPqGXZyp5Pmzb"
    "jpCn95i8tlnjfmiZaDjl3HxrY15zvw==";
constexpr char kAdDomainJoinUnlockedConfig[] = R"!!!(
[
  {
    "name": "Sales",
    "ad_username": "domain_join_account@example.com",
    "ad_password": "test123",
    "computer_ou": "OU=sales,DC=example,DC=com",
    "encryption_types": "all"
  },
  {
    "name": "Marketing",
    "ad_username": "domain_join_account@example.com",
    "ad_password": "test123",
    "computer_ou": "OU=marketing,DC=example,DC=com"
  },
  {
    "name": "Engineering",
    "ad_username": "other_domain_join_account@example.com",
    "ad_password": "test345",
    "computer_ou": "OU=engineering,DC=example,DC=com",
    "computer_name_validation_regex": "^DEVICE_\\d+$"
  }
]
)!!!";

class MockAuthPolicyClient : public FakeAuthPolicyClient {
 public:
  MockAuthPolicyClient() = default;
  ~MockAuthPolicyClient() override = default;
  void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                    int password_fd,
                    JoinCallback callback) override {
    if (expected_request_) {
      ASSERT_EQ(expected_request_->SerializeAsString(),
                request.SerializeAsString());
      expected_request_.reset();
    }
    FakeAuthPolicyClient::JoinAdDomain(request, password_fd,
                                       std::move(callback));
  }

  void set_expected_request(
      std::unique_ptr<authpolicy::JoinDomainRequest> expected_request) {
    expected_request_ = std::move(expected_request);
  }

 private:
  std::unique_ptr<authpolicy::JoinDomainRequest> expected_request_;
};

}  // namespace

class EnterpriseEnrollmentTestBase : public OobeBaseTest {
 public:
  EnterpriseEnrollmentTestBase() = default;

  // Submits regular enrollment credentials.
  void SubmitEnrollmentCredentials() {
    enrollment_screen()->OnLoginDone(
        "testuser@test.com", test::EnrollmentHelperMixin::kTestAuthCode);
    ExecutePendingJavaScript();
  }

  // Completes the enrollment process.
  void CompleteEnrollment() {
    enrollment_screen()->OnDeviceEnrolled();

    // Make sure all other pending JS calls have complete.
    ExecutePendingJavaScript();
  }

  // Makes sure that all pending JS calls have been executed. It is important
  // to make this a separate call from the DOM checks because JSChecker uses
  // a different IPC message for JS communication than the login code. This
  // means that the JS script ordering is not preserved between the login code
  // and the test code.
  void ExecutePendingJavaScript() { test::OobeJS().Evaluate(";"); }

  // Setup the enrollment screen.
  void ShowEnrollmentScreen() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    ASSERT_TRUE(host != nullptr);
    host->StartWizard(EnrollmentScreenView::kScreenId);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
    ASSERT_TRUE(enrollment_screen() != nullptr);
    ASSERT_TRUE(WizardController::default_controller() != nullptr);
  }

  // Helper method to return the current EnrollmentScreen instance.
  EnrollmentScreen* enrollment_screen() {
    return EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
  }

 protected:
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentTestBase);
};

class EnterpriseEnrollmentTest : public EnterpriseEnrollmentTestBase {
 public:
  EnterpriseEnrollmentTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentTest);
};

class ActiveDirectoryJoinTest : public EnterpriseEnrollmentTest {
 public:
  ActiveDirectoryJoinTest() = default;

  void SetUp() override {
    mock_authpolicy_client_ = new MockAuthPolicyClient();
    mock_authpolicy_client()->DisableOperationDelayForTesting();

    EnterpriseEnrollmentTestBase::SetUp();
  }

  std::string AdElement(const std::string& inner_id) {
    return test::GetOobeElementPath({kEnrollmentUI, kAdDialog, inner_id});
  }

  void ExpectElementValid(const std::string& inner_id, bool is_valid) {
    test::OobeJS().ExpectNE(AdElement(inner_id) + ".invalid", is_valid);
  }

  void CheckActiveDirectoryCredentialsShown() {
    enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepADJoin);

    std::initializer_list<base::StringPiece> ad_credentials{
        kEnrollmentUI, kAdDialog, kAdCredentialsStep};
    test::OobeJS().ExpectVisiblePath(ad_credentials);
    test::OobeJS().ExpectNE(
        test::GetOobeElementPath(ad_credentials) + ".clientWidth", 0);
    test::OobeJS().ExpectNE(
        test::GetOobeElementPath(ad_credentials) + ".clientHeight", 0);
    test::OobeJS().ExpectHiddenPath(
        {kEnrollmentUI, kAdDialog, kAdUnlockConfigurationStep});
  }

  void CheckConfigurationSelectionVisible(bool visible) {
    if (visible)
      test::OobeJS().ExpectVisiblePath(
          {kEnrollmentUI, kAdDialog, kAdJoinConfigurationForm});
    else
      test::OobeJS().ExpectHiddenPath(
          {kEnrollmentUI, kAdDialog, kAdJoinConfigurationForm});
  }

  void CheckActiveDirectoryUnlockConfigurationShown() {
    enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepADJoin);
    test::OobeJS().ExpectHiddenPath(
        {kEnrollmentUI, kAdDialog, kAdCredentialsStep});
    test::OobeJS().ExpectVisiblePath(
        {kEnrollmentUI, kAdDialog, kAdUnlockConfigurationStep});
  }

  void CheckAttributeValue(const base::Value* config_value,
                           const std::string& default_value,
                           const std::string& js_element) {
    std::string expected_value(default_value);
    if (config_value)
      expected_value = config_value->GetString();
    test::OobeJS().ExpectTrue(js_element + " === '" + expected_value + "'");
  }

  void CheckAttributeValueAndDisabled(const base::Value* config_value,
                                      const std::string& default_value,
                                      const std::string& js_element) {
    CheckAttributeValue(config_value, default_value, js_element + ".value");
    const bool is_disabled = bool(config_value);
    test::OobeJS().ExpectEQ(js_element + ".disabled", is_disabled);
  }

  // Checks pattern attribute on the machine name input field. If |config_value|
  // is nullptr the attribute should be undefined.
  void CheckPatternAttribute(const base::Value* config_value) {
    if (config_value) {
      std::string escaped_pattern;
      // Escape regex pattern.
      EXPECT_TRUE(base::EscapeJSONString(config_value->GetString(),
                                         false /* put_in_quotes */,
                                         &escaped_pattern));
      test::OobeJS().ExpectTrue(AdElement(kAdMachineNameInput) +
                                ".pattern  === '" + escaped_pattern + "'");
    } else {
      test::OobeJS().ExpectTrue("typeof " + AdElement(kAdMachineNameInput) +
                                ".pattern === 'undefined'");
    }
  }

  // Goes through |configuration| which is JSON (see
  // kAdDomainJoinUnlockedConfig). Selects each of them and checks that all the
  // input fields are set correctly. Also checks if there is a "Custom" option
  // which does not set any fields.
  void CheckPossibleConfiguration(const std::string& configuration) {
    std::unique_ptr<base::ListValue> options =
        base::ListValue::From(base::JSONReader::ReadDeprecated(
            configuration,
            base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
    base::DictionaryValue custom_option;
    custom_option.SetKey("name", base::Value("Custom"));
    options->Append(std::move(custom_option));
    for (size_t i = 0; i < options->GetList().size(); ++i) {
      const base::Value& option = options->GetList()[i];
      // Select configuration value.
      test::OobeJS().SelectElementInPath(
          base::NumberToString(i),
          {kEnrollmentUI, kAdDialog, kAdConfigurationSelect});

      CheckAttributeValue(
          option.FindKeyOfType("name", base::Value::Type::STRING), "",
          AdElement(kAdConfigurationSelect) + ".selectedOptions[0].label");

      CheckAttributeValueAndDisabled(
          option.FindKeyOfType("ad_username", base::Value::Type::STRING), "",
          AdElement(kAdUsernameInput));

      CheckAttributeValueAndDisabled(
          option.FindKeyOfType("ad_password", base::Value::Type::STRING), "",
          AdElement(kAdPasswordInput));

      CheckAttributeValueAndDisabled(
          option.FindKeyOfType("computer_ou", base::Value::Type::STRING), "",
          AdElement(kAdMachineOrgUnitInput));

      CheckAttributeValueAndDisabled(
          option.FindKeyOfType("encryption_types", base::Value::Type::STRING),
          "strong", AdElement(kAdEncryptionTypesSelect));

      CheckPatternAttribute(option.FindKeyOfType(
          "computer_name_validation_regex", base::Value::Type::STRING));
    }
  }

  // Submits Active Directory domain join credentials.
  void SubmitActiveDirectoryCredentials(const std::string& machine_name,
                                        const std::string& machine_dn,
                                        const std::string& encryption_types,
                                        const std::string& username,
                                        const std::string& password) {
    CheckActiveDirectoryCredentialsShown();

    test::OobeJS().TypeIntoPath(
        machine_name, {kEnrollmentUI, kAdDialog, kAdMachineNameInput});
    test::OobeJS().TypeIntoPath(username,
                                {kEnrollmentUI, kAdDialog, kAdUsernameInput});
    test::OobeJS().TypeIntoPath(password,
                                {kEnrollmentUI, kAdDialog, kAdPasswordInput});
    test::OobeJS().TapOnPath({kEnrollmentUI, kAdDialog, kAdMoreOptionsButton});
    test::OobeJS().TypeIntoPath(
        machine_dn, {kEnrollmentUI, kAdDialog, kAdMachineOrgUnitInput});

    if (!encryption_types.empty()) {
      test::OobeJS().SelectElementInPath(
          encryption_types,
          {kEnrollmentUI, kAdDialog, kAdEncryptionTypesSelect});
    }
    test::OobeJS().TapOnPath(
        {kEnrollmentUI, kAdDialog, kAdMoreOptionsSaveButton});
    test::OobeJS()
        .CreateEnabledWaiter(true /* enabled */,
                             {kEnrollmentUI, kAdDialog, kNextButton})
        ->Wait();
    test::OobeJS().TapOnPath({kEnrollmentUI, kAdDialog, kNextButton});
  }

  void SetExpectedJoinRequest(
      const std::string& machine_name,
      const std::string& machine_domain,
      authpolicy::KerberosEncryptionTypes encryption_types,
      std::vector<std::string> organizational_unit,
      const std::string& username,
      const std::string& dm_token) {
    auto request = std::make_unique<authpolicy::JoinDomainRequest>();
    if (!machine_name.empty())
      request->set_machine_name(machine_name);
    if (!machine_domain.empty())
      request->set_machine_domain(machine_domain);
    for (std::string& it : organizational_unit)
      request->add_machine_ou()->swap(it);
    if (!username.empty())
      request->set_user_principal_name(username);
    if (!dm_token.empty())
      request->set_dm_token(dm_token);
    request->set_kerberos_encryption_types(encryption_types);
    mock_authpolicy_client()->set_expected_request(std::move(request));
  }

  MockAuthPolicyClient* mock_authpolicy_client() {
    return mock_authpolicy_client_;
  }

  void SetupActiveDirectoryJSNotifications() {
    test::OobeJS().ExecuteAsync(
        "var originalShowStep = login.OAuthEnrollmentScreen.showStep;\n"
        "login.OAuthEnrollmentScreen.showStep = function(step) {\n"
        "  originalShowStep(step);\n"
        "  if (step == 'working') {\n"
        "    window.domAutomationController.send('ShowSpinnerScreen');\n"
        "  }"
        "}\n"
        "var originalShowError = login.OAuthEnrollmentScreen.showError;\n"
        "login.OAuthEnrollmentScreen.showError = function(message, retry) {\n"
        "  originalShowError(message, retry);\n"
        "  window.domAutomationController.send('ShowADJoinError');\n"
        "}\n");
    test::OobeJS().ExecuteAsync(
        "var originalSetAdJoinParams ="
        "    login.OAuthEnrollmentScreen.setAdJoinParams;"
        "login.OAuthEnrollmentScreen.setAdJoinParams = function("
        "    machineName, user, errorState, showUnlockConfig) {"
        "  originalSetAdJoinParams("
        "      machineName, user, errorState, showUnlockConfig);"
        "  window.domAutomationController.send('ShowJoinDomainError');"
        "}");
    test::OobeJS().ExecuteAsync(
        "var originalSetAdJoinConfiguration ="
        "    login.OAuthEnrollmentScreen.setAdJoinConfiguration;"
        "login.OAuthEnrollmentScreen.setAdJoinConfiguration = function("
        "    options) {"
        "  originalSetAdJoinConfiguration(options);"
        "  window.domAutomationController.send('SetAdJoinConfiguration');"
        "}");
  }

  void WaitForMessage(content::DOMMessageQueue* message_queue,
                      const std::string& expected_message) {
    std::string message;
    do {
      ASSERT_TRUE(message_queue->WaitForMessage(&message));
    } while (message != expected_message);
  }

 private:
  // Owned by the AuthPolicyClient global instance.
  MockAuthPolicyClient* mock_authpolicy_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryJoinTest);
};

// Shows the enrollment screen and simulates an enrollment complete event. We
// verify that the enrollmenth helper receives the correct auth code.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestAuthCodeGetsProperlyReceivedFromGaia) {
  ShowEnrollmentScreen();
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_MANUAL);
  enrollment_helper_.ExpectEnrollmentCredentials();
  enrollment_helper_.SetupClearAuth();

  SubmitEnrollmentCredentials();
}

// Shows the enrollment screen and simulates an enrollment failure. Verifies
// that the error screen is displayed.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestProperPageGetsLoadedOnEnrollmentFailure) {
  ShowEnrollmentScreen();

  enrollment_screen()->OnEnrollmentError(policy::EnrollmentStatus::ForStatus(
      policy::EnrollmentStatus::REGISTRATION_FAILED));
  ExecutePendingJavaScript();

  // Verify that the error page is displayed.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
}

// Shows the enrollment screen and simulates a successful enrollment. Verifies
// that the success screen is then displayed.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestProperPageGetsLoadedOnEnrollmentSuccess) {
  ShowEnrollmentScreen();
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_MANUAL);
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  SubmitEnrollmentCredentials();

  // Verify that the success page is displayed.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Shows the enrollment screen and mocks the enrollment helper to request an
// attribute prompt screen. Verifies the attribute prompt screen is displayed.
// Verifies that the data the user enters into the attribute prompt screen is
// received by the enrollment helper.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestAttributePromptPageGetsLoaded) {
  ShowEnrollmentScreen();
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_MANUAL);
  enrollment_helper_.ExpectAttributePromptUpdate(test::values::kAssetId,
                                                 test::values::kLocation);
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  SubmitEnrollmentCredentials();

  // Make sure the attribute-prompt view is open.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);

  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Verifies that the storage partition is updated when the enrollment screen is
// shown again.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest, StoragePartitionUpdated) {
  ShowEnrollmentScreen();
  ExecutePendingJavaScript();

  std::string webview_partition_path =
      test::GetOobeElementPath({kEnrollmentUI, kWebview}) + kPartitionAttribute;
  std::string webview_partition_name_1 =
      test::OobeJS().GetString(webview_partition_path);
  EXPECT_FALSE(webview_partition_name_1.empty());

  // Cancel button is enabled when the authenticator is ready. Do it manually
  // instead of waiting for it.
  test::ExecuteOobeJS("$('enterprise-enrollment').isCancelDisabled = false");
  test::OobeJS().ClickOnPath({kEnrollmentUI, kBackButton});

  // Simulate navigating over the enrollment screen a second time.
  ShowEnrollmentScreen();
  ExecutePendingJavaScript();

  // Verify that the partition name changes.
  const std::string partition_valid_and_changed_condition = base::StringPrintf(
      "%s && (%s != '%s')", webview_partition_path.c_str(),
      webview_partition_path.c_str(), webview_partition_name_1.c_str());
  test::OobeJS().CreateWaiter(partition_valid_and_changed_condition)->Wait();
}

// Shows the enrollment screen and mocks the enrollment helper to show Active
// Directory domain join screen. Verifies the domain join screen is displayed.
// Submits Active Directory credentials. Verifies that the AuthpolicyClient
// calls us back with the correct realm.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryJoinTest,
                       TestActiveDirectoryEnrollment_Success) {
  ShowEnrollmentScreen();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupActiveDirectoryJoin(
      enrollment_screen(), kAdUserDomain, std::string(), kDMToken);
  SubmitEnrollmentCredentials();

  chromeos::UpstartClient::Get()->StartAuthPolicyService();

  CheckActiveDirectoryCredentialsShown();
  CheckConfigurationSelectionVisible(false);
  content::DOMMessageQueue message_queue;
  SetupActiveDirectoryJSNotifications();
  SetExpectedJoinRequest("machine_name", "" /* machine_domain */,
                         authpolicy::KerberosEncryptionTypes::ENC_TYPES_ALL,
                         {} /* machine_ou */, kAdTestUser, kDMToken);
  SubmitActiveDirectoryCredentials("machine_name", "" /* machine_dn */, "all",
                                   kAdTestUser, "password");
  WaitForMessage(&message_queue, "\"ShowSpinnerScreen\"");
  enrollment_ui_.ExpectStepVisibility(false, test::ui::kEnrollmentStepADJoin);

  CompleteEnrollment();
  // Verify that the success page is displayed.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Verifies that the distinguished name specified on the Active Directory join
// domain screen correctly parsed and passed into AuthPolicyClient.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryJoinTest,
                       TestActiveDirectoryEnrollment_DistinguishedName) {
  ShowEnrollmentScreen();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupActiveDirectoryJoin(
      enrollment_screen(), kAdMachineDomain, std::string(), kDMToken);

  SubmitEnrollmentCredentials();

  chromeos::UpstartClient::Get()->StartAuthPolicyService();

  content::DOMMessageQueue message_queue;
  SetupActiveDirectoryJSNotifications();
  SetExpectedJoinRequest(
      "machine_name", kAdMachineDomain,
      authpolicy::KerberosEncryptionTypes::ENC_TYPES_STRONG,
      std::vector<std::string>(
          kAdOrganizationlUnit,
          kAdOrganizationlUnit + base::size(kAdOrganizationlUnit)),
      kAdTestUser, kDMToken);
  SubmitActiveDirectoryCredentials("machine_name", kAdMachineDomainDN,
                                   "" /* encryption_types */, kAdTestUser,
                                   "password");
  WaitForMessage(&message_queue, "\"ShowSpinnerScreen\"");
  enrollment_ui_.ExpectStepVisibility(false, test::ui::kEnrollmentStepADJoin);

  CompleteEnrollment();
  // Verify that the success page is displayed.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Shows the enrollment screen and mocks the enrollment helper to show Active
// Directory domain join screen. Verifies the domain join screen is displayed.
// Submits Active Directory different incorrect credentials. Verifies that the
// correct error is displayed.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryJoinTest,
                       TestActiveDirectoryEnrollment_UIErrors) {
  ShowEnrollmentScreen();
  enrollment_helper_.SetupActiveDirectoryJoin(
      enrollment_screen(), kAdUserDomain, std::string(), kDMToken);
  SubmitEnrollmentCredentials();

  chromeos::UpstartClient::Get()->StartAuthPolicyService();

  content::DOMMessageQueue message_queue;
  // Checking error in case of empty password. Whether password is not empty
  // being checked in the UI. Machine name length is checked after that in the
  // authpolicyd.
  SetupActiveDirectoryJSNotifications();
  SubmitActiveDirectoryCredentials("too_long_machine_name", "" /* machine_dn */,
                                   "" /* encryption_types */, kAdTestUser,
                                   "" /* password */);
  enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepADJoin);
  ExpectElementValid(kAdMachineNameInput, true);
  ExpectElementValid(kAdUsernameInput, true);
  ExpectElementValid(kAdPasswordInput, false);

  // Checking error in case of too long machine name.
  SubmitActiveDirectoryCredentials("too_long_machine_name", "" /* machine_dn */,
                                   "" /* encryption_types */, kAdTestUser,
                                   "password");
  WaitForMessage(&message_queue, "\"ShowJoinDomainError\"");
  enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepADJoin);
  ExpectElementValid(kAdMachineNameInput, false);
  ExpectElementValid(kAdUsernameInput, true);
  ExpectElementValid(kAdPasswordInput, true);

  // Checking error in case of bad username (without realm).
  SubmitActiveDirectoryCredentials("machine_name", "" /* machine_dn */,
                                   "" /* encryption_types */, "test_user",
                                   "password");
  WaitForMessage(&message_queue, "\"ShowJoinDomainError\"");
  enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepADJoin);
  ExpectElementValid(kAdMachineNameInput, true);
  ExpectElementValid(kAdUsernameInput, false);
  ExpectElementValid(kAdPasswordInput, true);
}

// Check that correct error card is shown (Active Directory one). Also checks
// that hitting retry shows Active Directory screen again.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryJoinTest,
                       TestActiveDirectoryEnrollment_ErrorCard) {
  ShowEnrollmentScreen();
  enrollment_helper_.SetupActiveDirectoryJoin(
      enrollment_screen(), kAdUserDomain, std::string(), kDMToken);
  SubmitEnrollmentCredentials();

  chromeos::UpstartClient::Get()->StartAuthPolicyService();

  content::DOMMessageQueue message_queue;
  SetupActiveDirectoryJSNotifications();
  // Legacy type triggers error card.
  SubmitActiveDirectoryCredentials("machine_name", "" /* machine_dn */,
                                   "legacy", kAdTestUser, "password");
  WaitForMessage(&message_queue, "\"ShowADJoinError\"");
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  test::OobeJS().ClickOnPath({kEnrollmentUI, kAdErrorButton});
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepADJoin);
}

// Check that configuration for the streamline Active Directory domain join
// propagates correctly to the Domain Join UI.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryJoinTest,
                       TestActiveDirectoryEnrollment_Streamline) {
  ShowEnrollmentScreen();
  std::string binary_config;
  EXPECT_TRUE(base::Base64Decode(kAdDomainJoinEncryptedConfig, &binary_config));
  enrollment_helper_.SetupActiveDirectoryJoin(
      enrollment_screen(), kAdUserDomain, binary_config, kDMToken);
  SubmitEnrollmentCredentials();

  chromeos::UpstartClient::Get()->StartAuthPolicyService();

  ExecutePendingJavaScript();
  content::DOMMessageQueue message_queue;
  SetupActiveDirectoryJSNotifications();

  // Unlock password step should we shown.
  CheckActiveDirectoryUnlockConfigurationShown();
  ExpectElementValid(kAdUnlockPasswordInput, true);

  // Test skipping the password step and getting back.
  test::OobeJS().TapOnPath({kEnrollmentUI, kAdDialog, kSkipButton});
  CheckActiveDirectoryCredentialsShown();
  CheckConfigurationSelectionVisible(false);
  test::OobeJS().ClickOnPath({kEnrollmentUI, kAdDialog, kAdBackToUnlockButton});
  CheckActiveDirectoryUnlockConfigurationShown();

  // Enter wrong unlock password.
  test::OobeJS().TypeIntoPath(
      "wrong_password", {kEnrollmentUI, kAdDialog, kAdUnlockPasswordInput});
  test::OobeJS().TapOnPath({kEnrollmentUI, kAdDialog, kAdUnlockButton});
  WaitForMessage(&message_queue, "\"ShowJoinDomainError\"");
  ExpectElementValid(kAdUnlockPasswordInput, false);

  // Enter right unlock password.
  test::OobeJS().TypeIntoPath(
      "test765!", {kEnrollmentUI, kAdDialog, kAdUnlockPasswordInput});
  test::OobeJS().TapOnPath({kEnrollmentUI, kAdDialog, kAdUnlockButton});
  WaitForMessage(&message_queue, "\"SetAdJoinConfiguration\"");
  CheckActiveDirectoryCredentialsShown();
  // Configuration selector should be visible.
  CheckConfigurationSelectionVisible(true);

  // Go through configuration.
  CheckPossibleConfiguration(kAdDomainJoinUnlockedConfig);
}

}  // namespace chromeos

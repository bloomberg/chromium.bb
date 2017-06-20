// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_impl.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "chromeos/dbus/upstart_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace chromeos {

namespace {

const char kAdMachineNameInput[] =
    "document.querySelector('#oauth-enroll-ad-join-ui /deep/ "
    "#machineNameInput')";
const char kAdUsernameInput[] =
    "document.querySelector('#oauth-enroll-ad-join-ui /deep/ #userInput')";
const char kAdPasswordInput[] =
    "document.querySelector('#oauth-enroll-ad-join-ui /deep/ #passwordInput')";
const char kAdTestRealm[] = "test_realm.com";
const char kAdTestUser[] = "test_user@test_realm.com";

}  // namespace

class EnterpriseEnrollmentTest : public LoginManagerTest {
 public:
  EnterpriseEnrollmentTest()
      : LoginManagerTest(true /*should_launch_browser*/) {
    enrollment_setup_functions_.clear();

    EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock([](
        EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
        const policy::EnrollmentConfig& enrollment_config,
        const std::string& enrolling_user_domain) {

      auto* mock = new EnterpriseEnrollmentHelperMock(status_consumer);
      for (OnSetupEnrollmentHelper fn : enrollment_setup_functions_)
        fn(mock);
      return (EnterpriseEnrollmentHelper*)mock;
    });
  }

  using OnSetupEnrollmentHelper =
      std::function<void(EnterpriseEnrollmentHelperMock*)>;

  // The given function will be executed when the next enrollment helper is
  // created.
  void AddEnrollmentSetupFunction(OnSetupEnrollmentHelper on_setup) {
    enrollment_setup_functions_.push_back(on_setup);
  }

  // Set up expectations for enrollment credentials.
  void ExpectEnrollmentCredentials() {
    AddEnrollmentSetupFunction([](
        EnterpriseEnrollmentHelperMock* enrollment_helper) {
      EXPECT_CALL(*enrollment_helper, EnrollUsingAuthCode("test_auth_code", _));

      ON_CALL(*enrollment_helper, ClearAuth(_))
          .WillByDefault(
              Invoke([](const base::Closure& callback) { callback.Run(); }));
    });
  }

  // Submits regular enrollment credentials.
  void SubmitEnrollmentCredentials() {
    // Trigger an authCompleted event from the authenticator.
    js_checker().Evaluate(
      "$('oauth-enrollment').authenticator_.dispatchEvent("
          "new CustomEvent('authCompleted',"
                          "{"
                            "detail: {"
                              "email: 'testuser@test.com',"
                              "authCode: 'test_auth_code'"
                            "}"
                          "}));");
  }

  // Submits Active Directory domain join credentials.
  void SubmitActiveDirectoryCredentials(const std::string& machine_name,
                                        const std::string& username,
                                        const std::string& password) {
    EXPECT_TRUE(IsStepDisplayed("ad-join"));
    js_checker().ExpectFalse(std::string(kAdMachineNameInput) + ".hidden");
    js_checker().ExpectFalse(std::string(kAdUsernameInput) + ".hidden");
    js_checker().ExpectFalse(std::string(kAdPasswordInput) + ".hidden");
    const std::string set_machine_name =
        std::string(kAdMachineNameInput) + ".value = '" + machine_name + "'";
    const std::string set_username =
        std::string(kAdUsernameInput) + ".value = '" + username + "'";
    const std::string set_password =
        std::string(kAdPasswordInput) + ".value = '" + password + "'";
    js_checker().ExecuteAsync(set_machine_name);
    js_checker().ExecuteAsync(set_username);
    js_checker().ExecuteAsync(set_password);
    js_checker().Evaluate(
        "document.querySelector('#oauth-enroll-ad-join-ui /deep/ "
        "#button').fire('tap')");
    ExecutePendingJavaScript();
  }

  void DisableAttributePromptUpdate() {
    AddEnrollmentSetupFunction(
        [](EnterpriseEnrollmentHelperMock* enrollment_helper) {
          EXPECT_CALL(*enrollment_helper, GetDeviceAttributeUpdatePermission())
              .WillOnce(InvokeWithoutArgs([enrollment_helper]() {
                enrollment_helper->status_consumer()
                    ->OnDeviceAttributeUpdatePermission(false);
              }));
        });
  }

  // Forces an attribute prompt to display.
  void ExpectAttributePromptUpdate() {
    AddEnrollmentSetupFunction(
        [](EnterpriseEnrollmentHelperMock* enrollment_helper) {
          // Causes the attribute-prompt flow to activate.
          ON_CALL(*enrollment_helper, GetDeviceAttributeUpdatePermission())
              .WillByDefault(InvokeWithoutArgs([enrollment_helper]() {
                enrollment_helper->status_consumer()
                    ->OnDeviceAttributeUpdatePermission(true);
              }));

          // Ensures we receive the updates attributes.
          EXPECT_CALL(*enrollment_helper,
                      UpdateDeviceAttributes("asset_id", "location"));
        });
  }

  // Forces the Active Directory domain join flow during enterprise enrollment.
  void SetupActiveDirectoryJoin() {
    AddEnrollmentSetupFunction([this](
        EnterpriseEnrollmentHelperMock* enrollment_helper) {
      // Causes the attribute-prompt flow to activate.
      EXPECT_CALL(*enrollment_helper, EnrollUsingAuthCode("test_auth_code", _))
          .WillOnce(InvokeWithoutArgs([this]() {
            this->enrollment_screen()->JoinDomain(base::BindOnce([](
                const std::string& realm) { EXPECT_EQ(kAdTestRealm, realm); }));
          }));
    });
    static_cast<FakeAuthPolicyClient*>(
        DBusThreadManager::Get()->GetAuthPolicyClient())
        ->set_operation_delay(base::TimeDelta::FromSeconds(0));
  }

  void SetupActiveDirectoryJSNotifications() {
    js_checker().Evaluate(
        "var testShowStep = login.OAuthEnrollmentScreen.showStep;"
        "login.OAuthEnrollmentScreen.showStep = function(step) {"
        "  testShowStep(step);"
        "  if (step == 'working') {"
        "    window.domAutomationController.setAutomationId(0);"
        "    window.domAutomationController.send('ShowSpinnerScreen');"
        "  }"
        "}");
    js_checker().Evaluate(
        "var testInvalidateAd = login.OAuthEnrollmentScreen.invalidateAd;"
        "login.OAuthEnrollmentScreen.invalidateAd = function(machineName, "
        "user, errorState) {"
        "  testInvalidateAd(machineName, user, errorState);"
        "  window.domAutomationController.setAutomationId(0);"
        "  window.domAutomationController.send('ShowJoinDomainError');"
        "}");
  }

  void WaitForMessage(content::DOMMessageQueue* message_queue,
                      const std::string& expected_message) {
    std::string message;
    do {
      ASSERT_TRUE(message_queue->WaitForMessage(&message));
    } while (message != expected_message);
  }

  // Fills out the UI with device attribute information and submits it.
  void SubmitAttributePromptUpdate() {
    // Fill out the attribute prompt info and submit it.
    js_checker().ExecuteAsync("$('oauth-enroll-asset-id').value = 'asset_id'");
    js_checker().ExecuteAsync("$('oauth-enroll-location').value = 'location'");
    js_checker().Evaluate(
        "$('oauth-enroll-attribute-prompt-card').fire('submit')");
  }

  // Completes the enrollment process.
  void CompleteEnrollment() {
    enrollment_screen()->OnDeviceEnrolled(std::string());

    // Make sure all other pending JS calls have complete.
    ExecutePendingJavaScript();
  }

  // Makes sure that all pending JS calls have been executed. It is important
  // to make this a separate call from the DOM checks because js_checker uses
  // a different IPC message for JS communication than the login code. This
  // means that the JS script ordering is not preserved between the login code
  // and the test code.
  void ExecutePendingJavaScript() { js_checker().Evaluate(";"); }

  // Returns true if there are any DOM elements with the given class.
  bool IsStepDisplayed(const std::string& step) {
    const std::string js =
        "document.getElementsByClassName('oauth-enroll-state-" + step +
        "').length";
    int count = js_checker().GetInt(js);
    return count > 0;
  }

  // Setup the enrollment screen.
  void ShowEnrollmentScreen() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    ASSERT_TRUE(host != nullptr);
    host->StartWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
    ASSERT_TRUE(enrollment_screen() != nullptr);
    ASSERT_TRUE(WizardController::default_controller() != nullptr);
    ASSERT_FALSE(StartupUtils::IsOobeCompleted());
  }

  // Helper method to return the current EnrollmentScreen instance.
  EnrollmentScreen* enrollment_screen() {
    return EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
  }

 private:
  static std::vector<OnSetupEnrollmentHelper> enrollment_setup_functions_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentTest);
};

std::vector<EnterpriseEnrollmentTest::OnSetupEnrollmentHelper>
    EnterpriseEnrollmentTest::enrollment_setup_functions_;

// Shows the enrollment screen and simulates an enrollment complete event. We
// verify that the enrollmenth helper receives the correct auth code.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestAuthCodeGetsProperlyReceivedFromGaia) {
  ShowEnrollmentScreen();
  ExpectEnrollmentCredentials();
  SubmitEnrollmentCredentials();

  // We need to reset enrollment_screen->enrollment_helper_, otherwise we will
  // get some errors on shutdown.
  enrollment_screen()->enrollment_helper_.reset();
}

// Shows the enrollment screen and simulates an enrollment failure. Verifies
// that the error screen is displayed.
// TODO(crbug.com/690634): Disabled due to timeout flakiness.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       DISABLED_TestProperPageGetsLoadedOnEnrollmentFailure) {
  ShowEnrollmentScreen();

  enrollment_screen()->OnEnrollmentError(policy::EnrollmentStatus::ForStatus(
      policy::EnrollmentStatus::REGISTRATION_FAILED));
  ExecutePendingJavaScript();

  // Verify that the error page is displayed.
  EXPECT_TRUE(IsStepDisplayed("error"));
  EXPECT_FALSE(IsStepDisplayed("success"));
}

// Shows the enrollment screen and simulates a successful enrollment. Verifies
// that the success screen is then displayed.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestProperPageGetsLoadedOnEnrollmentSuccess) {
  ShowEnrollmentScreen();
  DisableAttributePromptUpdate();
  SubmitEnrollmentCredentials();
  CompleteEnrollment();

  // Verify that the success page is displayed.
  EXPECT_TRUE(IsStepDisplayed("success"));
  EXPECT_FALSE(IsStepDisplayed("error"));

  // We have to remove the enrollment_helper before the dtor gets called.
  enrollment_screen()->enrollment_helper_.reset();
}

// Shows the enrollment screen and mocks the enrollment helper to request an
// attribute prompt screen. Verifies the attribute prompt screen is displayed.
// Verifies that the data the user enters into the attribute prompt screen is
// received by the enrollment helper.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestAttributePromptPageGetsLoaded) {
  ShowEnrollmentScreen();
  ExpectAttributePromptUpdate();
  SubmitEnrollmentCredentials();
  CompleteEnrollment();

  // Make sure the attribute-prompt view is open.
  EXPECT_TRUE(IsStepDisplayed("attribute-prompt"));
  EXPECT_FALSE(IsStepDisplayed("success"));
  EXPECT_FALSE(IsStepDisplayed("error"));

  SubmitAttributePromptUpdate();

  // We have to remove the enrollment_helper before the dtor gets called.
  enrollment_screen()->enrollment_helper_.reset();
}

// Shows the enrollment screen and mocks the enrollment helper to show Active
// Directory domain join screen. Verifies the domain join screen is displayed.
// Submits Active Directory credentials. Verifies that the AuthpolicyClient
// calls us back with the correct realm.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestActiveDirectoryEnrollment_Success) {
  ShowEnrollmentScreen();
  DisableAttributePromptUpdate();
  SetupActiveDirectoryJoin();
  SubmitEnrollmentCredentials();

  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->StartAuthPolicyService();

  content::DOMMessageQueue message_queue;
  SetupActiveDirectoryJSNotifications();
  SubmitActiveDirectoryCredentials("machine_name", kAdTestUser, "password");
  WaitForMessage(&message_queue, "\"ShowSpinnerScreen\"");
  EXPECT_FALSE(IsStepDisplayed("ad-join"));

  CompleteEnrollment();
  // Verify that the success page is displayed.
  EXPECT_TRUE(IsStepDisplayed("success"));
  EXPECT_FALSE(IsStepDisplayed("error"));

  // We have to remove the enrollment_helper before the dtor gets called.
  enrollment_screen()->enrollment_helper_.reset();
}

// Shows the enrollment screen and mocks the enrollment helper to show Active
// Directory domain join screen. Verifies the domain join screen is displayed.
// Submits Active Directory different incorrect credentials. Verifies that the
// correct error is displayed.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentTest,
                       TestActiveDirectoryEnrollment_UIErrors) {
  ShowEnrollmentScreen();
  SetupActiveDirectoryJoin();
  SubmitEnrollmentCredentials();

  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->StartAuthPolicyService();

  content::DOMMessageQueue message_queue;
  // Checking error in case of empty password. Whether password is not empty
  // being checked in the UI. Machine name length is checked after that in the
  // authpolicyd.
  SetupActiveDirectoryJSNotifications();
  SubmitActiveDirectoryCredentials("too_long_machine_name", kAdTestUser, "");
  EXPECT_TRUE(IsStepDisplayed("ad-join"));
  js_checker().ExpectFalse(std::string(kAdMachineNameInput) + ".isInvalid");
  js_checker().ExpectFalse(std::string(kAdUsernameInput) + ".isInvalid");
  js_checker().ExpectTrue(std::string(kAdPasswordInput) + ".isInvalid");

  // Checking error in case of too long machine name.
  SubmitActiveDirectoryCredentials("too_long_machine_name", kAdTestUser,
                                   "password");
  WaitForMessage(&message_queue, "\"ShowJoinDomainError\"");
  EXPECT_TRUE(IsStepDisplayed("ad-join"));
  js_checker().ExpectTrue(std::string(kAdMachineNameInput) + ".isInvalid");
  js_checker().ExpectFalse(std::string(kAdUsernameInput) + ".isInvalid");
  js_checker().ExpectFalse(std::string(kAdPasswordInput) + ".isInvalid");

  // Checking error in case of bad username (without realm).
  SubmitActiveDirectoryCredentials("machine_name", "test_user", "password");
  WaitForMessage(&message_queue, "\"ShowJoinDomainError\"");
  EXPECT_TRUE(IsStepDisplayed("ad-join"));
  js_checker().ExpectFalse(std::string(kAdMachineNameInput) + ".isInvalid");
  js_checker().ExpectTrue(std::string(kAdUsernameInput) + ".isInvalid");
  js_checker().ExpectFalse(std::string(kAdPasswordInput) + ".isInvalid");

  // We have to remove the enrollment_helper before the dtor gets called.
  enrollment_screen()->enrollment_helper_.reset();
}

}  // namespace chromeos

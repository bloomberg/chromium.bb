// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/core/common/policy_switches.h"

namespace chromeos {

namespace {

constexpr char kTestDomain[] = "test-domain.com";

void OnScreenExit(base::OnceClosure closure,
                  EnrollmentScreen::Result expected,
                  EnrollmentScreen::Result actual) {
  EXPECT_EQ(expected, actual);
  std::move(closure).Run();
}

EnrollmentScreen::ScreenExitCallback UserCannotSkipCallback(
    base::OnceClosure closure) {
  return base::AdaptCallbackForRepeating(base::BindOnce(
      OnScreenExit, std::move(closure), EnrollmentScreen::Result::BACK));
}

}  // namespace

class EnrollmentLocalPolicyServerBase : public OobeBaseTest {
 public:
  EnrollmentLocalPolicyServerBase() = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId,
                                     FakeGaiaMixin::kFakeRefreshToken);
    OobeBaseTest::SetUpOnMainThread();
  }

 protected:
  LoginDisplayHost* host() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    EXPECT_NE(host, nullptr);
    return host;
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_NE(enrollment_screen, nullptr);
    return enrollment_screen;
  }

  AutoEnrollmentCheckScreen* auto_enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    AutoEnrollmentCheckScreen* auto_enrollment_screen =
        AutoEnrollmentCheckScreen::Get(
            WizardController::default_controller()->screen_manager());
    EXPECT_NE(auto_enrollment_screen, nullptr);
    return auto_enrollment_screen;
  }

  LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentLocalPolicyServerBase);
};

class AutoEnrollmentLocalPolicyServer : public EnrollmentLocalPolicyServerBase {
 public:
  AutoEnrollmentLocalPolicyServer() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentLocalPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "5");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "5");
  }

  policy::ServerBackedStateKeysBroker* state_keys_broker() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetStateKeysBroker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentLocalPolicyServer);
};

// Simple manual enrollment.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase, ManualEnrollment) {
  host()->StartWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

  ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                   FakeGaiaMixin::kFakeAuthCode);

  test::OobeJS()
      .CreateWaiter(
          "document.getElementsByClassName('oauth-enroll-state-attribute-"
          "prompt').length > 0")
      ->Wait();
  // TODO(rsorokin): Interact with attribute prompt step.
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// No state keys on the server. Auto enrollment check should proceed to login.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, AutoEnrollmentCheck) {
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// State keys are present but restore mode is not requested.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentNone) {
  EXPECT_TRUE(local_policy_mixin_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// Reenrollment requested. User can skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentRequested) {
  EXPECT_TRUE(local_policy_mixin_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  enrollment_screen()->OnCancel();
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// Reenrollment forced. User can not skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentForced) {
  EXPECT_TRUE(local_policy_mixin_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  base::RunLoop loop;
  enrollment_screen()->set_exit_callback_for_testing(
      UserCannotSkipCallback(loop.QuitClosure()));
  enrollment_screen()->OnCancel();
  loop.Run();
}

// Device is disabled.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, DeviceDisabled) {
  EXPECT_TRUE(local_policy_mixin_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_DISABLED,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_DEVICE_DISABLED).Wait();
}

// TODO(rsorokin): Add test for RESTORE_MODE_REENROLLMENT_ZERO_TOUCH.

}  // namespace chromeos

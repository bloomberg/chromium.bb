// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
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
#include "chrome/grit/generated_resources.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/strings/grit/components_strings.h"

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
    policy_server_.SetUpdateDeviceAttributesPermission(false);
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

  void TriggerEnrollmentAndSignInSuccessfully() {
    host()->StartWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

    ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
    ASSERT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
    enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeAuthCode);
  }

  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_{&mixin_host_,
                                 DeviceStateMixin::State::BEFORE_OOBE};

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
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with device attributes prompt.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with only license type available.
// Client should automatically select the only available license type,
// so no license selection UI should be displayed.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithSingleLicense) {
  policy_server_.ExpectAvailableLicenseCount(5 /* perpetual */, 0 /* annual */,
                                             0 /* kiosk */);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with license selection.
// Enrollment selection UI should be displayed during enrollment.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithMultipleLicenses) {
  policy_server_.ExpectAvailableLicenseCount(5 /* perpetual */, 5 /* annual */,
                                             5 /* kiosk */);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepLicenses);
  enrollment_ui_.SelectEnrollmentLicense(test::values::kLicenseTypeAnnual);
  enrollment_ui_.UseSelectedLicense();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Negative scenarios: see different HTTP error codes in
// device_management_service.cc

// Error during enrollment : 402 - missing licenses.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorNoLicenses) {
  policy_server_.SetExpectedDeviceEnrollmentError(402);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 403 - management not allowed.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorManagementNotAllowed) {
  policy_server_.SetExpectedDeviceEnrollmentError(403);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 405 - invalid device serial.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorInvalidDeviceSerial) {
  policy_server_.SetExpectedDeviceEnrollmentError(405);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 406 - domain mismatch
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorDomainMismatch) {
  policy_server_.SetExpectedDeviceEnrollmentError(406);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 409 - Device ID is already in use
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorDeviceIDConflict) {
  policy_server_.SetExpectedDeviceEnrollmentError(409);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 412 - Activation is pending
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorActivationIsPending) {
  policy_server_.SetExpectedDeviceEnrollmentError(412);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_ACTIVATION_PENDING, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 417 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorConsumerAccountWithPackagedLicense) {
  policy_server_.SetExpectedDeviceEnrollmentError(417);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 500 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorServerError) {
  policy_server_.SetExpectedDeviceEnrollmentError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Strange HTTP response from server.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorServerIsDrunk) {
  policy_server_.SetExpectedDeviceEnrollmentError(12345);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_HTTP_STATUS_ERROR,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Can not update device attributes
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorUploadingDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);
  policy_server_.SetExpectedDeviceAttributeUpdateError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributesError);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
  enrollment_ui_.LeaveDeviceAttributeErrorScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// Error during enrollment : Error fetching policy : 500 server error.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyTransient) {
  policy_server_.SetExpectedPolicyFetchError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 902 - policy not found.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyNotFound) {
  policy_server_.SetExpectedPolicyFetchError(902);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_POLICY_NOT_FOUND,
      /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 903 - deprovisioned.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyDeprovisioned) {
  policy_server_.SetExpectedPolicyFetchError(903);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_SERVICE_DEPROVISIONED,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// No state keys on the server. Auto enrollment check should proceed to login.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, AutoEnrollmentCheck) {
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// State keys are present but restore mode is not requested.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentNone) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

// Reenrollment requested. User can skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentRequested) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
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
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
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
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_DISABLED,
      kTestDomain));
  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  OobeScreenWaiter(OobeScreen::SCREEN_DEVICE_DISABLED).Wait();
}

// Attestation enrollment.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, Attestation) {
  policy_server_.SetFakeAttestationFlow();
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      kTestDomain));

  host()->StartWizard(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

}  // namespace chromeos

// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/chromeos/login/test/hid_controller_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/input_method_util.h"

namespace chromeos {

// This test case will use
// src/chromeos/test/data/oobe_configuration/<TestName>.json file as
// OOBE configuration for each of the tests and verify that relevant parts
// of OOBE automation took place. OOBE WebUI will not be started until
// LoadConfiguration() is called to allow configure relevant stubs.
class EnterpriseEnrollmentConfigurationTest : public OobeBaseTest {
 public:
  EnterpriseEnrollmentConfigurationTest() = default;

  bool ShouldWaitForOobeUI() override { return false; }

  void LoadConfiguration() {
    OobeConfiguration::set_skip_check_for_testing(false);
    // Make sure configuration is loaded
    base::RunLoop run_loop;
    OOBEConfigurationWaiter waiter;

    OobeConfiguration::Get()->CheckConfiguration();

    const bool ready = waiter.IsConfigurationLoaded(run_loop.QuitClosure());
    if (!ready)
      run_loop.Run();

    // Let screens to settle.
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOfflineEnvironment() {
    DemoSetupController* controller =
        WizardController::default_controller()->demo_setup_controller();

    // Simulate offline data directory.
    ASSERT_TRUE(
        chromeos::test::SetupDummyOfflinePolicyDir("test", &fake_policy_dir_));
    controller->SetPreinstalledOfflineResourcesPathForTesting(
        fake_policy_dir_.GetPath());
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    OobeConfiguration::set_skip_check_for_testing(true);
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();

    fake_update_engine_client_ = new chromeos::FakeUpdateEngineClient();

    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<chromeos::UpdateEngineClient>(
            fake_update_engine_client_));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // File name is based on the test name.
    base::FilePath file;
    ASSERT_TRUE(GetTestFileName(".json", &file));

    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   file);

    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // Stores a name of the configuration data file to |file|.
  // Returns true iff |file| exists.
  bool GetTestFileName(const std::string& suffix, base::FilePath* file) {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string filename = std::string(test_info->name()) + suffix;
    return test_utils::GetTestDataPath("oobe_configuration", filename, file);
  }

  // Overridden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Set up fake networks.
    // TODO(pmarko): Find a way for FakeShillManagerClient to be initialized
    // automatically (https://crbug.com/847422).
    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();

    OobeBaseTest::SetUpOnMainThread();
    LoadConfiguration();

    // Make sure that OOBE is run as an "official" build.
    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");
  }

  // Returns true if there are any OAuth-Enroll DOM elements with the given
  // class.
  bool IsStepDisplayed(const std::string& step) {
    const std::string js =
        "document.getElementsByClassName('oauth-enroll-state-" + step +
        "').length";
    int count = test::OobeJS().GetInt(js);
    return count > 0;
  }

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};

  // Owned by DBusThreadManagerSetter
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;

  base::ScopedTempDir fake_policy_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentConfigurationTest);
};

// EnterpriseEnrollmentConfigurationTest with no input devices.
class EnterpriseEnrollmentConfigurationTestNoHID
    : public EnterpriseEnrollmentConfigurationTest {
 public:
  EnterpriseEnrollmentConfigurationTestNoHID() = default;

  ~EnterpriseEnrollmentConfigurationTestNoHID() override = default;

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentConfigurationTestNoHID);
};

// Check that configuration lets correctly pass Welcome screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestLeaveWelcomeScreen) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
}

// Check that language and input methods are set correctly.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestSwitchLanguageIME) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();

  // Configuration specified in TestSwitchLanguageIME.json sets non-default
  // input method fo German (xkb:de:neo:ger) to ensure that input method value
  // is propagated correctly. We need to migrate public IME name to internal
  // scheme to be able to compare them.

  const std::string ime_id =
      imm->GetInputMethodUtil()->MigrateInputMethod("xkb:de:neo:ger");
  EXPECT_EQ(ime_id, imm->GetActiveIMEState()->GetCurrentInputMethod().id());

  const std::string language_code = g_browser_process->local_state()->GetString(
      language::prefs::kApplicationLocale);
  EXPECT_EQ("de", language_code);
}

// Check that configuration lets correctly start Demo mode setup.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestEnableDemoMode) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
}

// Check that configuration lets correctly pass through demo preferences.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestDemoModePreferences) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
}

// Check that configuration lets correctly use offline demo mode on network
// screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestDemoModeOfflineNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  SimulateOfflineEnvironment();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
}

// Check that configuration lets correctly use offline demo mode on EULA
// screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestDemoModeAcceptEula) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  SimulateOfflineEnvironment();
  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
}

// Check that configuration lets correctly use offline demo mode on ARC++ ToS
// screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestDemoModeAcceptArcTos) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  SimulateOfflineEnvironment();

  test::OobeJS().Evaluate(
      "login.ArcTermsOfServiceScreen.setTosForTesting('Test "
      "Play Store Terms of Service');");
  test::OobeJS().Evaluate(
      "$('demo-preferences-content').$$('oobe-dialog')."
      "querySelector('oobe-text-button').click();");

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
}

// Check that configuration lets correctly select a network by GUID.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestSelectNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
}

// Check that configuration would proceed if there is a connected network.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestSelectConnectedNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
}

// Check that configuration would not proceed with connected network if
// welcome screen is not automated.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestConnectedNetworkNoWelcome) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

// Check that when configuration has ONC and EULA, we get to update screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest, TestAcceptEula) {
  UpdateEngineClient::Status status;
  status.status = UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 0.1;
  fake_update_engine_client_->set_default_status(status);

  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_UPDATE).Wait();
}

// Check that configuration allows to skip Update screen and get to Enrollment
// screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest, TestSkipUpdate) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  EXPECT_TRUE(IsStepDisplayed("signin"));
}

// Check that when configuration has requisition, it gets applied at the
// beginning.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestDeviceRequisition) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  auto* policy_manager = g_browser_process->platform_part()
                             ->browser_policy_connector_chromeos()
                             ->GetDeviceCloudPolicyManager();
  EXPECT_EQ(policy_manager->GetDeviceRequisition(), "some_requisition");
}

// Check that configuration allows to skip Update screen and get to Enrollment
// screen.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTest,
                       TestEnrollUsingToken) {
  enrollment_helper_.DisableAttributePromptUpdate();
  // Token from configuration file:
  enrollment_helper_.ExpectTokenEnrollmentSuccess(
      "00000000-1111-2222-3333-444444444444");
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  test::OobeJS().Evaluate(";");
  EXPECT_TRUE(IsStepDisplayed("success"));
}

// Check that HID detection screen is shown if it is not specified by
// configuration.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTestNoHID,
                       TestLeaveWelcomeScreen) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
}

// Check that HID detection screen is really skipped and rest of configuration
// is applied.
IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentConfigurationTestNoHID,
                       TestSkipHIDDetection) {
  LoadConfiguration();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
}

}  // namespace chromeos

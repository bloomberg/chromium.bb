// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"

namespace chromeos {

namespace {

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "test-user1@gmail.com";

void InvokeRollbackOption() {
  test::ExecuteOobeJS("cr.ui.Oobe.handleAccelerator('reset');");
}

void CloseResetScreen() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['cancel-reset']);");
}

void ClickResetButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['powerwash-pressed']);");
}

void ClickRestartButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['restart-pressed']);");
}

void ClickToConfirmButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', ['show-confirmation']);");
}

void ClickDismissConfirmationButton() {
  test::ExecuteOobeJS(
      "chrome.send('login.ResetScreen.userActed', "
      "['reset-confirm-dismissed']);");
}

}  // namespace

class ResetTest : public MixinBasedInProcessBrowserTest {
 public:
  ResetTest() = default;
  ~ResetTest() override = default;

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(update_engine_client_));

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  FakeUpdateEngineClient* update_engine_client_ = nullptr;

  // Simulates reset screen request from views based login.
  void InvokeResetScreen() {
    chromeos::LoginDisplayHost::default_host()->ShowResetScreen();
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  }

 private:
  LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)}};
  DISALLOW_COPY_AND_ASSIGN(ResetTest);
};

class ResetOobeTest : public OobeBaseTest {
 public:
  ResetOobeTest() = default;
  ~ResetOobeTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(update_engine_client_));

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  // Simulates reset screen request from OOBE UI.
  void InvokeResetScreen() {
    test::ExecuteOobeJS("cr.ui.Oobe.handleAccelerator('reset');");
  }

 protected:
  FakeUpdateEngineClient* update_engine_client_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResetOobeTest);
};

class ResetFirstAfterBootTest : public ResetTest {
 public:
  ~ResetFirstAfterBootTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }
};

class ResetFirstAfterBootTestWithRollback : public ResetTest {
 public:
  ~ResetFirstAfterBootTestWithRollback() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }
  void SetUpInProcessBrowserTestFixture() override {
    ResetTest::SetUpInProcessBrowserTestFixture();
    update_engine_client_->set_can_rollback_check_result(true);
  }
};

class ResetTestWithTpmFirmwareUpdate : public ResetTest {
 public:
  ResetTestWithTpmFirmwareUpdate() = default;
  ~ResetTestWithTpmFirmwareUpdate() override = default;

  // ResetTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResetTest::SetUpCommandLine(command_line);

    if (!content::IsPreTest())
      command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }

  void SetUpInProcessBrowserTestFixture() override {
    tpm_firmware_update_checker_callback_ = base::BindRepeating(
        &ResetTestWithTpmFirmwareUpdate::HandleTpmFirmwareUpdateCheck,
        base::Unretained(this));
    ResetScreen::SetTpmFirmwareUpdateCheckerForTesting(
        &tpm_firmware_update_checker_callback_);
    ResetTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ResetTest::TearDownInProcessBrowserTestFixture();
    ResetScreen::SetTpmFirmwareUpdateCheckerForTesting(nullptr);
  }

  bool HasPendingTpmFirmwareUpdateCheck() const {
    return !pending_tpm_firmware_update_check_.is_null();
  }

  void FinishPendingTpmFirmwareUpdateCheck(
      const std::set<tpm_firmware_update::Mode>& modes) {
    std::move(pending_tpm_firmware_update_check_).Run(modes);
  }

 private:
  void HandleTpmFirmwareUpdateCheck(
      ResetScreen::TpmFirmwareUpdateAvailabilityCallback callback,
      base::TimeDelta delay) {
    EXPECT_EQ(delay, base::TimeDelta::FromSeconds(10));
    // Multiple checks are technically allowed, but not needed by these tests.
    ASSERT_FALSE(pending_tpm_firmware_update_check_);
    pending_tpm_firmware_update_check_ = std::move(callback);
  }

  ResetScreen::TpmFirmwareUpdateAvailabilityCallback
      pending_tpm_firmware_update_check_;
  ResetScreen::TpmFirmwareUpdateAvailabilityChecker
      tpm_firmware_update_checker_callback_;
};

IN_PROC_BROWSER_TEST_F(ResetTest, ShowAndCancel) {
  InvokeResetScreen();
  test::OobeJS().ExpectVisible("reset");

  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"});
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforePowerwash) {
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  ClickRestartButton();
  ASSERT_EQ(1, FakePowerManagerClient::Get()->num_request_restart_calls());
  ASSERT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, ResetOnWelcomeScreen) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
  InvokeResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  test::OobeJS().ExpectVisible("reset");

  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetOobeTest, RequestAndCancleResetOnWelcomeScreen) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
  InvokeResetScreen();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  test::OobeJS().ExpectVisible("reset");

  CloseResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
  test::OobeJS().ExpectHidden("reset");

  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  update_engine_client_->set_can_rollback_check_result(false);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ViewsLogic) {
  PrefService* prefs = g_browser_process->local_state();

  // Rollback unavailable. Show and cancel.
  update_engine_client_->set_can_rollback_check_result(false);
  InvokeResetScreen();
  test::OobeJS().CreateVisibilityWaiter(true, {"reset"});
  test::OobeJS().ExpectHidden("overlay-reset");
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"});

  // Go to confirmation phase, cancel from there in 2 steps.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"});
  ClickToConfirmButton();
  test::OobeJS().CreateVisibilityWaiter(true, {"overlay-reset"});
  ClickDismissConfirmationButton();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"});
  test::OobeJS().CreateVisibilityWaiter(true, {"reset"});
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"});

  // Rollback available. Show and cancel from confirmation screen.
  update_engine_client_->set_can_rollback_check_result(true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();
  test::OobeJS().ExpectHidden("overlay-reset");
  ClickToConfirmButton();
  test::OobeJS().CreateVisibilityWaiter(true, {"overlay-reset"});
  ClickDismissConfirmationButton();
  test::OobeJS().CreateVisibilityWaiter(false, {"overlay-reset"});
  test::OobeJS().ExpectVisible("reset");
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"});
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ShowAfterBootIfRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ShowAfterBootIfRequested) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, {"reset"})->Wait();
  CloseResetScreen();
  test::OobeJS().CreateVisibilityWaiter(false, {"reset"})->Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_RollbackUnavailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, RollbackUnavailable) {
  InvokeResetScreen();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  InvokeRollbackOption();  // No changes
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  // Next invocation leads to rollback view.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback, RollbackAvailable) {
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  // Next invocation leads to simple reset, not rollback view.
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();  // Shows rollback.
  ClickDismissConfirmationButton();
  CloseResetScreen();
  OobeScreenExitWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  InvokeResetScreen();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenExitWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  InvokeResetScreen();
  InvokeRollbackOption();  // Shows rollback.
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(2, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_ErrorOnRollbackRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       ErrorOnRollbackRequested) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  test::OobeJS().ExpectHasNoClass("revert-promise-view", {"reset"});
  InvokeRollbackOption();
  ClickToConfirmButton();
  ClickResetButton();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
  test::OobeJS().ExpectHasClass("revert-promise-view", {"reset"});
  UpdateEngineClient::Status error_update_status;
  error_update_status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  update_engine_client_->NotifyObserversThatStatusChanged(error_update_status);
  OobeScreenWaiter(OobeScreen::SCREEN_ERROR_MESSAGE).Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback,
                       PRE_RevertAfterCancel) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTestWithRollback, RevertAfterCancel) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, FakePowerManagerClient::Get()->num_request_restart_calls());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());

  test::OobeJS().ExpectVisible("reset");
  test::OobeJS().ExpectHasNoClass("rollback-proposal-view", {"reset"});

  InvokeRollbackOption();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {"reset"})
      ->Wait();

  CloseResetScreen();
  OobeScreenExitWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  InvokeResetScreen();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  InvokeRollbackOption();
  test::OobeJS()
      .CreateHasClassWaiter(true, "rollback-proposal-view", {"reset"})
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetFromSigninWithFirmwareUpdate) {
  InvokeResetScreen();
  test::OobeJS().ExpectHiddenPath({"oobe-reset-md", "tpmFirmwareUpdate"});
  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS().ExpectHiddenPath({"oobe-reset-md", "tpmFirmwareUpdate"});
  ClickRestartButton();
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       ResetFromSigninWithFirmwareUpdate) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();
  test::OobeJS().Evaluate(
      test::GetOobeElementPath({"oobe-reset-md", "tpmFirmwareUpdateCheckbox"}) +
      ".fire('click')");

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("first_boot",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_TpmFirmwareUpdateAvailableButNotSelected) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       TpmFirmwareUpdateAvailableButNotSelected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kPowerwash});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetWithTpmCleanUp) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kCleanup));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate, ResetWithTpmCleanUp) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({tpm_firmware_update::Mode::kCleanup});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("cleanup",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_ResetWithTpmUpdatePreservingDeviceState) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(
      prefs::kFactoryResetTPMFirmwareUpdateMode,
      static_cast<int>(tpm_firmware_update::Mode::kPreserveDeviceState));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       ResetWithTpmUpdatePreservingDeviceState) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck(
      {tpm_firmware_update::Mode::kPreserveDeviceState,
       tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("preserve_stateful",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_TpmFirmwareUpdateRequestedBeforeShowNotEditable) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
}

// Tests that clicking TPM firmware update checkbox is no-op if the update was
// requested before the Reset screen was shown (e.g. on previous boot in
// settings, or by policy).
IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       TpmFirmwareUpdateRequestedBeforeShowNotEditable) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  test::OobeJS().Evaluate(
      test::GetOobeElementPath({"oobe-reset-md", "tpmFirmwareUpdateCheckbox"}) +
      ".fire('click')");

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck(
      {tpm_firmware_update::Mode::kPreserveDeviceState,
       tpm_firmware_update::Mode::kPowerwash});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      1,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
  EXPECT_EQ("first_boot",
            FakeSessionManagerClient::Get()->last_tpm_firmware_update_mode());
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       PRE_AvailableTpmUpdateModesChangeDuringRequest) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                    static_cast<int>(tpm_firmware_update::Mode::kPowerwash));
}

IN_PROC_BROWSER_TEST_F(ResetTestWithTpmFirmwareUpdate,
                       AvailableTpmUpdateModesChangeDuringRequest) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_RESET).Wait();

  EXPECT_FALSE(HasPendingTpmFirmwareUpdateCheck());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-reset-md", "tpmFirmwareUpdate"})
      ->Wait();

  ClickResetButton();
  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());

  ASSERT_TRUE(HasPendingTpmFirmwareUpdateCheck());
  FinishPendingTpmFirmwareUpdateCheck({});

  EXPECT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  EXPECT_EQ(
      0,
      FakeSessionManagerClient::Get()->start_tpm_firmware_update_call_count());
}

}  // namespace chromeos

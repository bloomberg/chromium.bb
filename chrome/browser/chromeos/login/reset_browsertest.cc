// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";

}  // namespace

class ResetTest : public LoginManagerTest {
 public:
  ResetTest() : LoginManagerTest(false),
      update_engine_client_(NULL),
      session_manager_client_(NULL),
      power_manager_client_(NULL) {
  }
  virtual ~ResetTest() {}

  // LoginManagerTest overrides:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    FakeDBusThreadManager* dbus_manager = new FakeDBusThreadManager;
    dbus_manager->SetFakeClients();
    session_manager_client_ = new FakeSessionManagerClient;
    dbus_manager->SetSessionManagerClient(
        scoped_ptr<SessionManagerClient>(session_manager_client_));
    power_manager_client_ = new FakePowerManagerClient;
    dbus_manager->SetPowerManagerClient(
        scoped_ptr<PowerManagerClient>(power_manager_client_));
    update_engine_client_ = new FakeUpdateEngineClient;
    dbus_manager->SetUpdateEngineClient(
        scoped_ptr<UpdateEngineClient>(update_engine_client_));
    DBusThreadManager::SetInstanceForTesting(dbus_manager);

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void RegisterSomeUser() {
    RegisterUser(kTestUser1);
    StartupUtils::MarkOobeCompleted();
  }

  bool JSExecuted(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  void InvokeResetScreen() {
    ASSERT_TRUE(JSExecuted("cr.ui.Oobe.handleAccelerator('reset');"));
    OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_RESET).Wait();
  }

  void CloseResetScreen() {
    ASSERT_TRUE(JSExecuted("$('reset-cancel-button').click();"));
  }

  void ClickResetButton() {
    ASSERT_TRUE(JSExecuted("$('reset-button').click();"));
  }

  FakeUpdateEngineClient* update_engine_client_;
  FakeSessionManagerClient* session_manager_client_;
  FakePowerManagerClient* power_manager_client_;
};

class ResetFirstAfterBootTest : public ResetTest {
 public:
  virtual ~ResetFirstAfterBootTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kFirstExecAfterBoot);
  }
};

IN_PROC_BROWSER_TEST_F(ResetTest, PRE_ShowAndCancel) {
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetTest, ShowAndCancel) {
  JSExpect("!!document.querySelector('#reset.hidden')");
  EXPECT_EQ(0, update_engine_client_->can_rollback_call_count());
  InvokeResetScreen();
  JSExpect("!document.querySelector('#reset.hidden')");
  CloseResetScreen();
  JSExpect("!!document.querySelector('#reset.hidden')");
  EXPECT_EQ(1, update_engine_client_->can_rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetTest, PRE_RestartBeforePowerwash) {
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforePowerwash) {
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  ClickResetButton();
  ASSERT_EQ(1, power_manager_client_->num_request_restart_calls());
  ASSERT_EQ(0, session_manager_client_->start_device_wipe_call_count());

  EXPECT_FALSE(prefs->GetBoolean(prefs::kRollbackRequested));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
}

IN_PROC_BROWSER_TEST_F(ResetTest, PRE_RestartBeforeRollback) {
  RegisterSomeUser();
  update_engine_client_->set_can_rollback_check_result(true);
}

IN_PROC_BROWSER_TEST_F(ResetTest, RestartBeforeRollback) {
  PrefService* prefs = g_browser_process->local_state();

  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(),
      "$('reset-rollback-checkbox').checked = true;"));
  ClickResetButton();
  ASSERT_EQ(1, power_manager_client_->num_request_restart_calls());
  ASSERT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());

  EXPECT_TRUE(prefs->GetBoolean(prefs::kRollbackRequested));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kFactoryResetRequested));
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ShortcutInvokedCases) {
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ShortcutInvokedCases) {
  // rollback unavailable
  update_engine_client_->set_can_rollback_check_result(false);
  InvokeResetScreen();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(1, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenWaiter(OobeDisplay::SCREEN_ACCOUNT_PICKER).Wait();

  update_engine_client_->set_can_rollback_check_result(true);
  // rollback available and unchecked
  InvokeResetScreen();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(2, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  CloseResetScreen();
  OobeScreenWaiter(OobeDisplay::SCREEN_ACCOUNT_PICKER).Wait();

  // rollback available and checked
  InvokeResetScreen();
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(),
      "$('reset-rollback-checkbox').checked = true;"));
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(2, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_PowerwashRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kRollbackRequested, false);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PowerwashRequested) {
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(1, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, PRE_ErrorOnRollbackRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kRollbackRequested, true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, ErrorOnRollbackRequested) {
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_RESET).Wait();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(0, update_engine_client_->rollback_call_count());
  JSExpect("!$('reset').classList.contains('revert-promise')");
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
  JSExpect("$('reset').classList.contains('revert-promise')");
  UpdateEngineClient::Status error_update_status;
  error_update_status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
  update_engine_client_->NotifyObserversThatStatusChanged(
      error_update_status);
  OobeScreenWaiter(OobeDisplay::SCREEN_ERROR_MESSAGE).Wait();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest,
                       PRE_SuccessOnRollbackRequested) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kRollbackRequested, true);
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  RegisterSomeUser();
}

IN_PROC_BROWSER_TEST_F(ResetFirstAfterBootTest, SuccessOnRollbackRequested) {
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_RESET).Wait();
  ClickResetButton();
  EXPECT_EQ(0, power_manager_client_->num_request_restart_calls());
  EXPECT_EQ(0, session_manager_client_->start_device_wipe_call_count());
  EXPECT_EQ(1, update_engine_client_->rollback_call_count());
  UpdateEngineClient::Status ready_for_reboot_status;
  ready_for_reboot_status.status =
      UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  update_engine_client_->NotifyObserversThatStatusChanged(
      ready_for_reboot_status);
  EXPECT_EQ(1, power_manager_client_->num_request_restart_calls());
}


}  // namespace chromeos

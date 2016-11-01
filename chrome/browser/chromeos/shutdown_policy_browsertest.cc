// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/common/login_status.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/strings/grit/ash_strings.h"
#include "ash/common/system/date/date_default_view.h"
#include "ash/common/system/date/tray_date.h"
#include "ash/common/system/tiles/tiles_default_view.h"
#include "ash/common/system/tiles/tray_tiles.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/view.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

const char kWaitForHiddenStateScript[] =
    "var screenElement = document.getElementById('%s');"
    "var expectation = %s;"
    "function SendReplyIfAsExpected() {"
    "  if (screenElement.hidden != expectation)"
    "    return false;"
    "  domAutomationController.send(screenElement.hidden);"
    "  observer.disconnect();"
    "  return true;"
    "}"
    "var observer = new MutationObserver(SendReplyIfAsExpected);"
    "if (!SendReplyIfAsExpected()) {"
    "  var options = { attributes: true };"
    "  observer.observe(screenElement, options);"
    "}";

}  // namespace

class ShutdownPolicyBaseTest
    : public policy::DevicePolicyCrosBrowserTest,
      public DeviceSettingsService::Observer {
 protected:
  ShutdownPolicyBaseTest() : contents_(nullptr) {}
  ~ShutdownPolicyBaseTest() override {}

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override {}
  void DeviceSettingsUpdated() override {
    if (run_loop_)
      run_loop_->Quit();
  }
  void OnDeviceSettingsServiceShutdown() override {}

  // policy::DevicePolicyCrosBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
  }

  // A helper functions which prepares the script by injecting the element_id of
  // the element whose hiddenness we want to check and the expectation.
  std::string PrepareScript(const std::string& element_id, bool expectation) {
    return base::StringPrintf(kWaitForHiddenStateScript, element_id.c_str(),
                              expectation ? "true" : "false");
  }

  // Checks whether the element identified by |element_id| is hidden and only
  // returns if the expectation is fulfilled.
  void PrepareAndRunScript(const std::string& element_id, bool expectation) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents_, PrepareScript(element_id, expectation),
        &result_));
  }

  // Updates the device shutdown policy and sets it to |reboot_on_shutdown|.
  void UpdateRebootOnShutdownPolicy(bool reboot_on_shutdown) {
    policy::DevicePolicyBuilder* builder = device_policy();
    ASSERT_TRUE(builder);
    em::ChromeDeviceSettingsProto& proto(builder->payload());
    proto.mutable_reboot_on_shutdown()->set_reboot_on_shutdown(
        reboot_on_shutdown);
  }

  // Refreshes device policy and waits for it to be applied.
  void SyncRefreshDevicePolicy() {
    run_loop_.reset(new base::RunLoop());
    DeviceSettingsService::Get()->AddObserver(this);
    RefreshDevicePolicy();
    run_loop_->Run();
    DeviceSettingsService::Get()->RemoveObserver(this);
    run_loop_.reset();
  }

  // Blocks until the OobeUI indicates that the javascript side has been
  // initialized.
  void WaitUntilOobeUIIsReady(OobeUI* oobe_ui) {
    ASSERT_TRUE(oobe_ui);
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready)
      run_loop.Run();
  }

  content::WebContents* contents_;
  bool result_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ShutdownPolicyInSessionTest
    : public ShutdownPolicyBaseTest {
 protected:
  ShutdownPolicyInSessionTest() {}
  ~ShutdownPolicyInSessionTest() override {}

  void SetUpOnMainThread() override {
    ShutdownPolicyBaseTest::SetUpOnMainThread();
    if (ash::MaterialDesignController::IsSystemTrayMenuMaterial()) {
      ash::TrayTiles* tray_tiles = ash::Shell::GetInstance()
                                       ->GetPrimarySystemTray()
                                       ->GetTrayTilesForTesting();
      ASSERT_TRUE(tray_tiles);
      tiles_default_view_.reset(static_cast<ash::TilesDefaultView*>(
          tray_tiles->CreateDefaultViewForTesting(ash::LoginStatus::USER)));
      ASSERT_TRUE(tiles_default_view_);
    } else {
      ash::TrayDate* tray_date = ash::Shell::GetInstance()
                                     ->GetPrimarySystemTray()
                                     ->GetTrayDateForTesting();
      ASSERT_TRUE(tray_date);
      date_default_view_.reset(static_cast<ash::DateDefaultView*>(
          tray_date->CreateDefaultViewForTesting(ash::LoginStatus::USER)));
      ASSERT_TRUE(date_default_view_);
    }
  }

  void TearDownOnMainThread() override {
    if (ash::MaterialDesignController::IsSystemTrayMenuMaterial())
      tiles_default_view_.reset();
    else
      date_default_view_.reset();
    ShutdownPolicyBaseTest::TearDownOnMainThread();
  }

  // Get the shutdown and reboot button view from the date default view.
  const views::CustomButton* GetShutdownButton() {
    if (ash::MaterialDesignController::IsSystemTrayMenuMaterial())
      return tiles_default_view_->GetShutdownButtonViewForTest();
    return static_cast<const views::CustomButton*>(
        date_default_view_->GetShutdownButtonViewForTest());
  }

  bool HasButtonTooltipText(const views::CustomButton* button,
                            int message_id) const {
    base::string16 actual_tooltip;
    button->GetTooltipText(gfx::Point(), &actual_tooltip);
    return l10n_util::GetStringUTF16(message_id) == actual_tooltip;
  }

 private:
  // Not used in material design.
  std::unique_ptr<ash::DateDefaultView> date_default_view_;

  // Only used in material design.
  std::unique_ptr<ash::TilesDefaultView> tiles_default_view_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyInSessionTest);
};

IN_PROC_BROWSER_TEST_F(ShutdownPolicyInSessionTest, TestBasic) {
  const views::CustomButton* shutdown_button = GetShutdownButton();
  EXPECT_TRUE(
      HasButtonTooltipText(shutdown_button, IDS_ASH_STATUS_TRAY_SHUTDOWN));
}

IN_PROC_BROWSER_TEST_F(ShutdownPolicyInSessionTest, PolicyChange) {
  const views::CustomButton* shutdown_button = GetShutdownButton();

  UpdateRebootOnShutdownPolicy(true);
  SyncRefreshDevicePolicy();
  EXPECT_TRUE(
      HasButtonTooltipText(shutdown_button, IDS_ASH_STATUS_TRAY_REBOOT));

  UpdateRebootOnShutdownPolicy(false);
  SyncRefreshDevicePolicy();
  EXPECT_TRUE(
      HasButtonTooltipText(shutdown_button, IDS_ASH_STATUS_TRAY_SHUTDOWN));
}

class ShutdownPolicyLockerTest : public ShutdownPolicyBaseTest {
 protected:
  ShutdownPolicyLockerTest() : fake_session_manager_client_(nullptr) {}
  ~ShutdownPolicyLockerTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));

    ShutdownPolicyBaseTest::SetUpInProcessBrowserTestFixture();
    zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
  }

  void SetUpOnMainThread() override {
    ShutdownPolicyBaseTest::SetUpOnMainThread();

    // Bring up the locker screen.
    ScreenLocker::Show();
    std::unique_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
    tester->EmulateWindowManagerReady();
    content::WindowedNotificationObserver lock_state_observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    if (!tester->IsLocked())
      lock_state_observer.Wait();
    ScreenLocker* screen_locker = ScreenLocker::default_screen_locker();
    WebUIScreenLocker* web_ui_screen_locker = screen_locker->web_ui();
    ASSERT_TRUE(web_ui_screen_locker);
    content::WebUI* web_ui = web_ui_screen_locker->GetWebUI();
    ASSERT_TRUE(web_ui);
    contents_ = web_ui->GetWebContents();
    ASSERT_TRUE(contents_);

    // Wait for the login UI to be ready.
    WaitUntilOobeUIIsReady(web_ui_screen_locker->GetOobeUI());
  }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyLockerTest);
};

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLockerTest, TestBasic) {
  PrepareAndRunScript("restart-header-bar-item", true);
  PrepareAndRunScript("shutdown-header-bar-item", false);
}

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLockerTest, PolicyChange) {
  UpdateRebootOnShutdownPolicy(true);
  RefreshDevicePolicy();
  PrepareAndRunScript("restart-header-bar-item", false);
  PrepareAndRunScript("shutdown-header-bar-item", true);

  UpdateRebootOnShutdownPolicy(false);
  RefreshDevicePolicy();
  PrepareAndRunScript("restart-header-bar-item", true);
  PrepareAndRunScript("shutdown-header-bar-item", false);
}

class ShutdownPolicyLoginTest : public ShutdownPolicyBaseTest {
 protected:
  ShutdownPolicyLoginTest() {}
  ~ShutdownPolicyLoginTest() override {}

  // ShutdownPolicyBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ShutdownPolicyBaseTest::SetUpInProcessBrowserTestFixture();
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
  }

  void SetUpOnMainThread() override {
    ShutdownPolicyBaseTest::SetUpOnMainThread();

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources()).Wait();
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    WebUILoginView* web_ui_login_view = host->GetWebUILoginView();
    ASSERT_TRUE(web_ui_login_view);
    content::WebUI* web_ui = web_ui_login_view->GetWebUI();
    ASSERT_TRUE(web_ui);
    contents_ = web_ui->GetWebContents();
    ASSERT_TRUE(contents_);

    // Wait for the login UI to be ready.
    WaitUntilOobeUIIsReady(host->GetOobeUI());
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyLoginTest);
};

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLoginTest, PolicyNotSet) {
  PrepareAndRunScript("restart-header-bar-item", true);
  PrepareAndRunScript("shutdown-header-bar-item", false);
}

IN_PROC_BROWSER_TEST_F(ShutdownPolicyLoginTest, PolicyChange) {
  UpdateRebootOnShutdownPolicy(true);
  RefreshDevicePolicy();
  PrepareAndRunScript("restart-header-bar-item", false);
  PrepareAndRunScript("shutdown-header-bar-item", true);

  UpdateRebootOnShutdownPolicy(false);
  RefreshDevicePolicy();
  PrepareAndRunScript("restart-header-bar-item", true);
  PrepareAndRunScript("shutdown-header-bar-item", false);
}

}  // namespace chromeos

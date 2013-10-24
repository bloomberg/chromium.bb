// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/app_launch_signin_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggbflgnkafappblpkiflbgpmkfdpnhhe
const char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

// This app creates a window and declares usage of the identity API in its
// manifest, so we can test device robot token minting via the identity API.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ibjkkfdnfcaoapcpheeijckmpcfkifob
const char kTestEnterpriseKioskApp[] = "ibjkkfdnfcaoapcpheeijckmpcfkifob";

// Timeout while waiting for network connectivity during tests.
const int kTestNetworkTimeoutSeconds = 1;

// Email of owner account for test.
const char kTestOwnerEmail[] = "owner@example.com";

const char kTestEnterpriseAccountId[] = "enterprise-kiosk-app@localhost";
const char kTestEnterpriseServiceAccountId[] = "service_account@example.com";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAccessToken[] = "fake-access-token";

// Helper function for GetConsumerKioskModeStatusCallback.
void ConsumerKioskModeStatusCheck(
    KioskAppManager::ConsumerKioskModeStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskModeStatus in_status) {
  LOG(INFO) << "KioskAppManager::ConsumerKioskModeStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeLockCheck(
    bool* out_locked,
    const base::Closure& runner_quit_task,
    bool in_locked) {
  LOG(INFO) << "kiosk locked  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

// Helper function for WaitForNetworkTimeOut.
void OnNetworkWaitTimedOut(const base::Closure& runner_quit_task) {
  runner_quit_task.Run();
}

// Helper function for DeviceOAuth2TokenServiceFactory::Get().
void CopyTokenService(DeviceOAuth2TokenService** out_token_service,
                      DeviceOAuth2TokenService* in_token_service) {
  *out_token_service = in_token_service;
}

}  // namespace

// Fake NetworkChangeNotifier used to simulate network connectivity.
class FakeNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  FakeNetworkChangeNotifier() : connection_type_(CONNECTION_NONE) {}

  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return connection_type_;
  }

  void GoOnline() {
    SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  }

  void GoOffline() {
    SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  void SetConnectionType(ConnectionType type) {
    connection_type_ = type;
    NotifyObserversOfNetworkChange(type);
    base::RunLoop().RunUntilIdle();
  }

  virtual ~FakeNetworkChangeNotifier() {}

 private:
  ConnectionType connection_type_;
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkChangeNotifier);
};

class KioskTest : public InProcessBrowserTest,
                  // Param defining is multi-profiles enabled.
                  public testing::WithParamInterface<bool> {
 public:
  KioskTest() {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~KioskTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    mock_user_manager_.reset(new MockUserManager);
    AppLaunchController::SkipSplashWaitForTesting();
    AppLaunchController::SetNetworkWaitForTesting(kTestNetworkTimeoutSeconds);

    InProcessBrowserTest::SetUp();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // We need to clean up these objects in this specific order.
    fake_network_notifier_.reset(NULL);
    disable_network_notifier_.reset(NULL);

    AppLaunchController::SetNetworkTimeoutCallbackForTesting(NULL);
    AppLaunchController::SetUserManagerForTesting(NULL);

    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }

    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    KioskAppManager::Get()->CleanUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (GetParam())
      command_line->AppendSwitch(::switches::kMultiProfiles);

    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(
        chromeos::switches::kDisableChromeCaptivePortalDetector);
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

    const GURL& server_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, server_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, server_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    server_url.spec());
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        server_url.Resolve("/chromeos/app_mode/webstore").spec());
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryDownloadURL,
        server_url.Resolve(
            "/chromeos/app_mode/webstore/downloads/%s.crx").spec());
  }

  void ReloadKioskApps() {
    KioskAppManager::Get()->AddApp(kTestKioskApp);
  }

  void ReloadAutolaunchKioskApps() {
    KioskAppManager::Get()->AddApp(kTestKioskApp);
    KioskAppManager::Get()->SetAutoLaunchApp(kTestKioskApp);
  }

  void StartAppLaunchFromLoginScreen(bool has_connectivity) {
    EnableConsumerKioskMode();

    // Start UI, find menu entry for this app and launch it.
    content::WindowedNotificationObserver login_signal(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources());
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    CHECK(wizard_controller);
    wizard_controller->SkipToLoginForTesting();
    login_signal.Wait();

    // Wait for the Kiosk App configuration to reload, then launch the app.
    content::WindowedNotificationObserver apps_loaded_signal(
        chrome::NOTIFICATION_KIOSK_APPS_LOADED,
        content::NotificationService::AllSources());
    ReloadKioskApps();
    apps_loaded_signal.Wait();

    if (!has_connectivity)
      SimulateNetworkOffline();

    GetLoginUI()->CallJavascriptFunction(
        "login.AppsMenuButton.runAppForTesting",
        base::StringValue(kTestKioskApp));
  }

  void WaitForAppLaunchSuccess() {
    SimulateNetworkOnline();

    ExtensionTestMessageListener
        launch_data_check_listener("launchData.isKioskSession = true", false);

    // Wait for the Kiosk App to launch.
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
        content::NotificationService::AllSources()).Wait();

    // Check installer status.
    EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
              chromeos::KioskAppLaunchError::Get());

    // Check if the kiosk webapp is really installed for the default profile.
    ASSERT_TRUE(ProfileManager::GetDefaultProfile());
    const extensions::Extension* app =
        extensions::ExtensionSystem::Get(ProfileManager::GetDefaultProfile())->
        extension_service()->GetInstalledExtension(kTestKioskApp);
    EXPECT_TRUE(app);

    // Wait until the app terminates.
    content::RunMessageLoop();

    // Check that the app had been informed that it is running in a kiosk
    // session.
    EXPECT_TRUE(launch_data_check_listener.was_satisfied());
  }

  void SimulateNetworkOffline() {
    disable_network_notifier_.reset(
        new net::NetworkChangeNotifier::DisableForTest);

    fake_network_notifier_.reset(new FakeNetworkChangeNotifier);
  }

  void SimulateNetworkOnline() {
    if (fake_network_notifier_.get())
      fake_network_notifier_->GoOnline();
  }

  void WaitForAppLaunchNetworkTimeout() {
    if (GetAppLaunchController()->network_wait_timedout())
      return;

    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    base::Closure callback = base::Bind(
        &OnNetworkWaitTimedOut, runner->QuitClosure());
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(&callback);

    runner->Run();

    CHECK(GetAppLaunchController()->network_wait_timedout());
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(NULL);
  }

  void EnableConsumerKioskMode() {
    scoped_ptr<bool> locked(new bool(false));
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->EnableConsumerModeKiosk(
        base::Bind(&ConsumerKioskModeLockCheck,
                   locked.get(),
                   runner->QuitClosure()));
    runner->Run();
    EXPECT_TRUE(*locked.get());
  }

  KioskAppManager::ConsumerKioskModeStatus GetConsumerKioskModeStatus() {
    KioskAppManager::ConsumerKioskModeStatus status =
        static_cast<KioskAppManager::ConsumerKioskModeStatus>(-1);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->GetConsumerKioskModeStatus(
        base::Bind(&ConsumerKioskModeStatusCheck,
                   &status,
                   runner->QuitClosure()));
    runner->Run();
    CHECK_NE(status, static_cast<KioskAppManager::ConsumerKioskModeStatus>(-1));
    return status;
  }

  content::WebUI* GetLoginUI() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
  }

  SigninScreenHandler* GetSigninScreenHandler() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())
        ->GetOobeUI()
        ->signin_screen_handler_for_test();
  }

  AppLaunchController* GetAppLaunchController() {
    return chromeos::LoginDisplayHostImpl::default_host()
        ->GetAppLaunchController();
  }

  FakeGaia fake_gaia_;
  scoped_ptr<net::NetworkChangeNotifier::DisableForTest>
      disable_network_notifier_;
  scoped_ptr<FakeNetworkChangeNotifier> fake_network_notifier_;
  scoped_ptr<MockUserManager> mock_user_manager_;
};

IN_PROC_BROWSER_TEST_P(KioskTest, InstallAndLaunchApp) {
  StartAppLaunchFromLoginScreen(true);
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_P(KioskTest, LaunchAppNetworkDown) {
  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(false);
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();
  WaitForAppLaunchNetworkTimeout();

  // Set up fake user manager with an owner for the test.
  mock_user_manager_->SetActiveUser(kTestOwnerEmail);
  AppLaunchController::SetUserManagerForTesting(mock_user_manager_.get());
  static_cast<LoginDisplayHostImpl*>(LoginDisplayHostImpl::default_host())
      ->GetOobeUI()->ShowOobeUI(false);

  // Configure network should bring up lock screen for owner.
  OobeScreenWaiter lock_screen_waiter(OobeDisplay::SCREEN_ACCOUNT_PICKER);
  static_cast<AppLaunchSplashScreenActor::Delegate*>(GetAppLaunchController())
    ->OnConfigureNetwork();
  lock_screen_waiter.Wait();

  // A network error screen should be shown after authenticating.
  OobeScreenWaiter error_screen_waiter(OobeDisplay::SCREEN_ERROR_MESSAGE);
  static_cast<AppLaunchSigninScreen::Delegate*>(GetAppLaunchController())
      ->OnOwnerSigninSuccess();
  error_screen_waiter.Wait();

  ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_P(KioskTest, LaunchAppUserCancel) {
  StartAppLaunchFromLoginScreen(false);
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();

  CrosSettings::Get()->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, true);
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("app_launch_bailout"));
  signal.Wait();
  EXPECT_EQ(chromeos::KioskAppLaunchError::USER_CANCEL,
            chromeos::KioskAppLaunchError::Get());
}

IN_PROC_BROWSER_TEST_P(KioskTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting();

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(false));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());
}

IN_PROC_BROWSER_TEST_P(KioskTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(true));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_TRUE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Check if the kiosk webapp is really installed for the default profile.
  ASSERT_TRUE(ProfileManager::GetDefaultProfile());
  const extensions::Extension* app =
      extensions::ExtensionSystem::Get(ProfileManager::GetDefaultProfile())->
      extension_service()->GetInstalledExtension(kTestKioskApp);
  EXPECT_TRUE(app);

  // Wait until the app terminates.
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_P(KioskTest, KioskEnableCancel) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(false));

  // Wait for the kiosk_enable screen to disappear.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  // Check that the status still says configurable.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_P(KioskTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());
  wizard_controller->SkipToLoginForTesting();

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(true));

  // Wait for the signal that indicates Kiosk Mode is enabled.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLED,
      content::NotificationService::AllSources()).Wait();
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_ENABLED,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_P(KioskTest, KioskEnableAbortedWithAutoEnrollment) {
  // Fake an auto enrollment is going to be enforced.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentInitialModulus, "1");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentModulusLimit, "2");
  g_browser_process->local_state()->SetBoolean(prefs::kShouldAutoEnroll, true);
  g_browser_process->local_state()->SetInteger(
      prefs::kAutoEnrollmentPowerLimit, 3);

  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());
  wizard_controller->SkipToLoginForTesting();

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // The flow should be aborted due to auto enrollment enforcement.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  GetSigninScreenHandler()->set_kiosk_enable_flow_aborted_callback_for_test(
      runner->QuitClosure());
  runner->Run();
}

INSTANTIATE_TEST_CASE_P(KioskTestInstantiation, KioskTest, testing::Bool());

// Helper class that monitors app windows to wait for a window to appear.
class ShellWindowObserver : public apps::ShellWindowRegistry::Observer {
 public:
  ShellWindowObserver(apps::ShellWindowRegistry* registry,
                      const std::string& app_id)
      : registry_(registry), app_id_(app_id), window_(NULL), running_(false) {
    registry_->AddObserver(this);
  }
  virtual ~ShellWindowObserver() {
    registry_->RemoveObserver(this);
  }

  apps::ShellWindow* Wait() {
    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(window_);
    return window_;
  }

  // ShellWindowRegistry::Observer
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE {
    if (!running_)
      return;

    if (shell_window->extension_id() == app_id_) {
      window_ = shell_window;
      message_loop_runner_->Quit();
      running_ = false;
    }
  }
  virtual void OnShellWindowIconChanged(
      apps::ShellWindow* shell_window) OVERRIDE {}
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) OVERRIDE {}

 private:
  apps::ShellWindowRegistry* registry_;
  std::string app_id_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  apps::ShellWindow* window_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowObserver);
};

class KioskEnterpriseTest : public KioskTest {
 protected:
  KioskEnterpriseTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    device_policy_test_helper_.MarkAsEnterpriseOwned();
    device_policy_test_helper_.InstallOwnerKey();

    KioskTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Configure kTestEnterpriseKioskApp in device policy.
    em::DeviceLocalAccountsProto* accounts =
        device_policy_test_helper_.device_policy()->payload()
            .mutable_device_local_accounts();
    em::DeviceLocalAccountInfoProto* account = accounts->add_account();
    account->set_account_id(kTestEnterpriseAccountId);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(kTestEnterpriseKioskApp);
    accounts->set_auto_login_id(kTestEnterpriseAccountId);
    em::PolicyData& policy_data =
        device_policy_test_helper_.device_policy()->policy_data();
    policy_data.set_service_account_identity(kTestEnterpriseServiceAccountId);
    device_policy_test_helper_.device_policy()->Build();
    DBusThreadManager::Get()->GetSessionManagerClient()->StoreDevicePolicy(
        device_policy_test_helper_.device_policy()->GetBlob(),
        base::Bind(&KioskEnterpriseTest::StorePolicyCallback));

    DeviceSettingsService::Get()->Load();

    // Configure OAuth authentication.
    FakeGaia::AccessTokenInfo token_info;
    token_info.token = kTestAccessToken;
    token_info.email = kTestEnterpriseServiceAccountId;
    fake_gaia_.IssueOAuthToken(kTestRefreshToken, token_info);
    DeviceOAuth2TokenService* token_service = NULL;
    DeviceOAuth2TokenServiceFactory::Get(
        base::Bind(&CopyTokenService, &token_service));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(token_service);
    token_service->SetAndSaveRefreshToken(kTestRefreshToken);

    KioskTest::SetUpOnMainThread();
  }

  static void StorePolicyCallback(bool result) {
    ASSERT_TRUE(result);
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskEnterpriseTest);
};

IN_PROC_BROWSER_TEST_P(KioskEnterpriseTest, EnterpriseKioskApp) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  wizard_controller->SkipToLoginForTesting();

  // Wait for the Kiosk App configuration to reload, then launch the app.
  KioskAppManager::App app;
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      base::Bind(&KioskAppManager::GetApp,
                 base::Unretained(KioskAppManager::Get()),
                 kTestEnterpriseKioskApp, &app)).Wait();

  GetLoginUI()->CallJavascriptFunction(
      "login.AppsMenuButton.runAppForTesting",
      base::StringValue(kTestEnterpriseKioskApp));

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Wait for the window to appear.
  apps::ShellWindow* window = ShellWindowObserver(
      apps::ShellWindowRegistry::Get(ProfileManager::GetDefaultProfile()),
      kTestEnterpriseKioskApp).Wait();
  ASSERT_TRUE(window);

  // Check whether the app can retrieve an OAuth2 access token.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      window->web_contents(),
      "chrome.identity.getAuthToken({ 'interactive': false }, function(token) {"
      "    window.domAutomationController.send(token);"
      "});",
      &result));
  EXPECT_EQ(kTestAccessToken, result);

  // Terminate the app.
  window->GetBaseWindow()->Close();
  content::RunAllPendingInMessageLoop();
}

// Disabled due to failures; http://crbug.com/306611.
INSTANTIATE_TEST_CASE_P(DISABLED_KioskEnterpriseTestInstantiation,
                        KioskEnterpriseTest,
                        testing::Bool());

}  // namespace chromeos

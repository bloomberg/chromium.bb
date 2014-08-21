// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/app_window_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_app_menu_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_system.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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

// An offline enable test app. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ajoggoflpgplnnjkjamcmbepjdjdnpdp
// An app profile with version 1.0.0 installed is in
//   chrome/test/data/chromeos/app_mode/offline_enabled_app_profile
// The version 2.0.0 crx is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
const char kTestOfflineEnabledKioskApp[] = "ajoggoflpgplnnjkjamcmbepjdjdnpdp";

// An app to test local fs data persistence across app update. V1 app writes
// data into local fs. V2 app reads and verifies the data.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/bmbpicmpniaclbbpdkfglgipkkebnbjf
const char kTestLocalFsKioskApp[] = "bmbpicmpniaclbbpdkfglgipkkebnbjf";

// Timeout while waiting for network connectivity during tests.
const int kTestNetworkTimeoutSeconds = 1;

// Email of owner account for test.
const char kTestOwnerEmail[] = "owner@example.com";

const char kTestEnterpriseAccountId[] = "enterprise-kiosk-app@localhost";
const char kTestEnterpriseServiceAccountId[] = "service_account@example.com";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestUserinfoToken[] = "fake-userinfo-token";
const char kTestLoginToken[] = "fake-login-token";
const char kTestAccessToken[] = "fake-access-token";
const char kTestClientId[] = "fake-client-id";
const char kTestAppScope[] =
    "https://www.googleapis.com/auth/userinfo.profile";

// Test JS API.
const char kLaunchAppForTestNewAPI[] =
    "login.AccountPickerScreen.runAppForTesting";
const char kLaunchAppForTestOldAPI[] =
    "login.AppsMenuButton.runAppForTesting";
const char kCheckDiagnosticModeNewAPI[] =
    "$('oobe').confirmDiagnosticMode_";
const char kCheckDiagnosticModeOldAPI[] =
    "$('show-apps-button').confirmDiagnosticMode_";

// Helper function for GetConsumerKioskAutoLaunchStatusCallback.
void ConsumerKioskAutoLaunchStatusCheck(
    KioskAppManager::ConsumerKioskAutoLaunchStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskAutoLaunchStatus in_status) {
  LOG(INFO) << "KioskAppManager::ConsumerKioskModeStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeAutoStartLockCheck(
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

// Helper function for LockFileThread.
void LockAndUnlock(scoped_ptr<base::Lock> lock) {
  lock->Acquire();
  lock->Release();
}

// Helper functions for CanConfigureNetwork mock.
class ScopedCanConfigureNetwork {
 public:
  ScopedCanConfigureNetwork(bool can_configure, bool needs_owner_auth)
      : can_configure_(can_configure),
        needs_owner_auth_(needs_owner_auth),
        can_configure_network_callback_(
            base::Bind(&ScopedCanConfigureNetwork::CanConfigureNetwork,
                       base::Unretained(this))),
        needs_owner_auth_callback_(base::Bind(
            &ScopedCanConfigureNetwork::NeedsOwnerAuthToConfigureNetwork,
            base::Unretained(this))) {
    AppLaunchController::SetCanConfigureNetworkCallbackForTesting(
        &can_configure_network_callback_);
    AppLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
        &needs_owner_auth_callback_);
  }
  ~ScopedCanConfigureNetwork() {
    AppLaunchController::SetCanConfigureNetworkCallbackForTesting(NULL);
    AppLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
        NULL);
  }

  bool CanConfigureNetwork() {
    return can_configure_;
  }

  bool NeedsOwnerAuthToConfigureNetwork() {
    return needs_owner_auth_;
  }

 private:
  bool can_configure_;
  bool needs_owner_auth_;
  AppLaunchController::ReturnBoolCallback can_configure_network_callback_;
  AppLaunchController::ReturnBoolCallback needs_owner_auth_callback_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCanConfigureNetwork);
};

// Helper class to wait until a js condition becomes true.
class JsConditionWaiter {
 public:
  JsConditionWaiter(content::WebContents* web_contents,
                    const std::string& js)
      : web_contents_(web_contents),
        js_(js) {
  }

  void Wait() {
    if (CheckJs())
      return;

    base::RepeatingTimer<JsConditionWaiter> check_timer;
    check_timer.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(10),
        this,
        &JsConditionWaiter::OnTimer);

    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  bool CheckJs() {
    bool result;
    CHECK(content::ExecuteScriptAndExtractBool(
        web_contents_,
        "window.domAutomationController.send(!!(" + js_ + "));",
        &result));
    return result;
  }

  void OnTimer() {
    DCHECK(runner_);
    if (CheckJs())
      runner_->Quit();
  }

  content::WebContents* web_contents_;
  const std::string js_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

}  // namespace

class KioskTest : public OobeBaseTest {
 public:
  KioskTest() : fake_cws_(new FakeCWS) {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~KioskTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    test_app_id_ = kTestKioskApp;
    set_test_app_version("1.0.0");
    set_test_crx_file(test_app_id() + ".crx");
    needs_background_networking_ = true;
    mock_user_manager_.reset(new MockUserManager);
    ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
    AppLaunchController::SkipSplashWaitForTesting();
    AppLaunchController::SetNetworkWaitForTesting(kTestNetworkTimeoutSeconds);

    OobeBaseTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
    OobeBaseTest::TearDown();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    OobeBaseTest::SetUpOnMainThread();
    // Needed to avoid showing Gaia screen instead of owner signin for
    // consumer network down test cases.
    StartupUtils::MarkDeviceRegistered(base::Closure());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(NULL);
    AppLaunchSigninScreen::SetUserManagerForTesting(NULL);

    OobeBaseTest::TearDownOnMainThread();

    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    KioskAppManager::Get()->CleanUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    OobeBaseTest::SetUpCommandLine(command_line);
    fake_cws_->Init(embedded_test_server());
  }

  void LaunchApp(const std::string& app_id, bool diagnostic_mode) {
    bool new_kiosk_ui = KioskAppMenuHandler::EnableNewKioskUI();
    GetLoginUI()->CallJavascriptFunction(new_kiosk_ui ?
        kLaunchAppForTestNewAPI : kLaunchAppForTestOldAPI,
        base::StringValue(app_id),
        base::FundamentalValue(diagnostic_mode));
  }

  void ReloadKioskApps() {
    SetupTestAppUpdateCheck();

    // Remove then add to ensure NOTIFICATION_KIOSK_APPS_LOADED fires.
    KioskAppManager::Get()->RemoveApp(test_app_id_);
    KioskAppManager::Get()->AddApp(test_app_id_);
  }

  void FireKioskAppSettingsChanged() {
    KioskAppManager::Get()->UpdateAppData();
  }

  void SetupTestAppUpdateCheck() {
    if (!test_app_version().empty()) {
      fake_cws_->SetUpdateCrx(
          test_app_id(), test_crx_file(), test_app_version());
    }
  }

  void ReloadAutolaunchKioskApps() {
    SetupTestAppUpdateCheck();

    KioskAppManager::Get()->AddApp(test_app_id_);
    KioskAppManager::Get()->SetAutoLaunchApp(test_app_id_);
  }

  void StartUIForAppLaunch() {
    EnableConsumerKioskMode();

    // Start UI
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    if (wizard_controller) {
      wizard_controller->SkipToLoginForTesting(LoginScreenContext());
      OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
    } else {
      // No wizard and running with an existing profile and it should land
      // on account picker when new kiosk UI is enabled. Otherwise, just
      // wait for the login signal from Gaia.
      if (KioskAppMenuHandler::EnableNewKioskUI())
        OobeScreenWaiter(OobeDisplay::SCREEN_ACCOUNT_PICKER).Wait();
      else
        OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
    }
  }

  void PrepareAppLaunch() {
    // Start UI
    StartUIForAppLaunch();

    // Wait for the Kiosk App configuration to reload.
    content::WindowedNotificationObserver apps_loaded_signal(
        chrome::NOTIFICATION_KIOSK_APPS_LOADED,
        content::NotificationService::AllSources());
    ReloadKioskApps();
    apps_loaded_signal.Wait();
  }

  void StartAppLaunchFromLoginScreen(const base::Closure& network_setup_cb) {
    PrepareAppLaunch();

    if (!network_setup_cb.is_null())
      network_setup_cb.Run();

    LaunchApp(test_app_id(), false);
  }

  const extensions::Extension* GetInstalledApp() {
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    return extensions::ExtensionSystem::Get(app_profile)->
        extension_service()->GetInstalledExtension(test_app_id_);
  }

  const Version& GetInstalledAppVersion() {
    return *GetInstalledApp()->version();
  }

  void WaitForAppLaunchSuccess() {
    ExtensionTestMessageListener
        launch_data_check_listener("launchData.isKioskSession = true", false);

    // Wait for the Kiosk App to launch.
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
        content::NotificationService::AllSources()).Wait();

    // Default profile switches to app profile after app is launched.
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    ASSERT_TRUE(app_profile);

    // Check ChromeOS preference is initialized.
    EXPECT_TRUE(
        static_cast<ProfileImpl*>(app_profile)->chromeos_preferences_);

    // Check installer status.
    EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
              chromeos::KioskAppLaunchError::Get());

    // Check if the kiosk webapp is really installed for the default profile.
    const extensions::Extension* app =
        extensions::ExtensionSystem::Get(app_profile)->
        extension_service()->GetInstalledExtension(test_app_id_);
    EXPECT_TRUE(app);

    // App should appear with its window.
    apps::AppWindowRegistry* app_window_registry =
        apps::AppWindowRegistry::Get(app_profile);
    apps::AppWindow* window =
        AppWindowWaiter(app_window_registry, test_app_id_).Wait();
    EXPECT_TRUE(window);

    // Login screen should be gone or fading out.
    chromeos::LoginDisplayHost* login_display_host =
        chromeos::LoginDisplayHostImpl::default_host();
    EXPECT_TRUE(
        login_display_host == NULL ||
        login_display_host->GetNativeWindow()->layer()->GetTargetOpacity() ==
            0.0f);

    // Wait until the app terminates if it is still running.
    if (!app_window_registry->GetAppWindowsForApp(test_app_id_).empty())
      content::RunMessageLoop();

    // Check that the app had been informed that it is running in a kiosk
    // session.
    EXPECT_TRUE(launch_data_check_listener.was_satisfied());
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
    KioskAppManager::Get()->EnableConsumerKioskAutoLaunch(
        base::Bind(&ConsumerKioskModeAutoStartLockCheck,
                   locked.get(),
                   runner->QuitClosure()));
    runner->Run();
    EXPECT_TRUE(*locked.get());
  }

  KioskAppManager::ConsumerKioskAutoLaunchStatus
  GetConsumerKioskModeStatus() {
    KioskAppManager::ConsumerKioskAutoLaunchStatus status =
        static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
        base::Bind(&ConsumerKioskAutoLaunchStatusCheck,
                   &status,
                   runner->QuitClosure()));
    runner->Run();
    CHECK_NE(status,
             static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1));
    return status;
  }

  void RunAppLaunchNetworkDownTest() {
    mock_user_manager()->SetActiveUser(kTestOwnerEmail);
    AppLaunchSigninScreen::SetUserManagerForTesting(mock_user_manager());

    // Mock network could be configured with owner's password.
    ScopedCanConfigureNetwork can_configure_network(true, true);

    // Start app launch and wait for network connectivity timeout.
    StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
    OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
    splash_waiter.Wait();
    WaitForAppLaunchNetworkTimeout();

    // Configure network link should be visible.
    JsExpect("$('splash-config-network').hidden == false");

    // Set up fake user manager with an owner for the test.
    static_cast<LoginDisplayHostImpl*>(LoginDisplayHostImpl::default_host())
        ->GetOobeUI()->ShowOobeUI(false);

    // Configure network should bring up lock screen for owner.
    OobeScreenWaiter lock_screen_waiter(OobeDisplay::SCREEN_ACCOUNT_PICKER);
    static_cast<AppLaunchSplashScreenActor::Delegate*>(GetAppLaunchController())
        ->OnConfigureNetwork();
    lock_screen_waiter.Wait();

    // There should be only one owner pod on this screen.
    JsExpect("$('pod-row').alwaysFocusSinglePod");

    // A network error screen should be shown after authenticating.
    OobeScreenWaiter error_screen_waiter(OobeDisplay::SCREEN_ERROR_MESSAGE);
    static_cast<AppLaunchSigninScreen::Delegate*>(GetAppLaunchController())
        ->OnOwnerSigninSuccess();
    error_screen_waiter.Wait();

    ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());

    SimulateNetworkOnline();
    WaitForAppLaunchSuccess();
  }

  AppLaunchController* GetAppLaunchController() {
    return chromeos::LoginDisplayHostImpl::default_host()
        ->GetAppLaunchController();
  }

  // Returns a lock that is holding a task on the FILE thread. Any tasks posted
  // to the FILE thread after this call will be blocked until the returned
  // lock is released.
  // This can be used to prevent app installation from completing until some
  // other conditions are checked and triggered. For example, this can be used
  // to trigger the network screen during app launch without racing with the
  // app launching process itself.
  scoped_ptr<base::AutoLock> LockFileThread() {
    scoped_ptr<base::Lock> lock(new base::Lock);
    scoped_ptr<base::AutoLock> auto_lock(new base::AutoLock(*lock));
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&LockAndUnlock, base::Passed(&lock)));
    return auto_lock.Pass();
  }

  MockUserManager* mock_user_manager() { return mock_user_manager_.get(); }

  void set_test_app_id(const std::string& test_app_id) {
    test_app_id_ = test_app_id;
  }
  const std::string& test_app_id() const { return test_app_id_; }
  void set_test_app_version(const std::string& version) {
    test_app_version_ = version;
  }
  const std::string& test_app_version() const { return test_app_version_; }
  void set_test_crx_file(const std::string& filename) {
    test_crx_file_ = filename;
  }
  const std::string& test_crx_file() const { return test_crx_file_; }
  FakeCWS* fake_cws() { return fake_cws_.get(); }

 private:
  std::string test_app_id_;
  std::string test_app_version_;
  std::string test_crx_file_;
  scoped_ptr<FakeCWS> fake_cws_;
  scoped_ptr<MockUserManager> mock_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(KioskTest);
};

IN_PROC_BROWSER_TEST_F(KioskTest, InstallAndLaunchApp) {
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, NotSignedInWithGAIAAccount) {
  // Tests that the kiosk session is not considered to be logged in with a GAIA
  // account.
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  WaitForAppLaunchSuccess();

  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);
  EXPECT_FALSE(app_profile->GetPrefs()->HasPrefPath(
      prefs::kGoogleServicesUsername));
}

IN_PROC_BROWSER_TEST_F(KioskTest, PRE_LaunchAppNetworkDown) {
  // Tests the network down case for the initial app download and launch.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkDown) {
  // Tests the network down case for launching an existing app that is
  // installed in PRE_LaunchAppNetworkDown.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppWithNetworkConfigAccelerator) {
  ScopedCanConfigureNetwork can_configure_network(true, false);

  // Block app loading until the network screen is shown.
  scoped_ptr<base::AutoLock> lock = LockFileThread();

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();

  // A network error screen should be shown after authenticating.
  OobeScreenWaiter error_screen_waiter(OobeDisplay::SCREEN_ERROR_MESSAGE);
  // Simulate Ctrl+Alt+N accelerator.
  GetLoginUI()->CallJavascriptFunction(
      "cr.ui.Oobe.handleAccelerator",
      base::StringValue("app_launch_network_config"));
  error_screen_waiter.Wait();
  ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());

  // Continue button should be visible since we are online.
  JsExpect("$('continue-network-config-btn').hidden == false");

  // Click on [Continue] button.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "(function() {"
      "var e = new Event('click');"
      "$('continue-network-config-btn').dispatchEvent(e);"
      "})();"));

  // Let app launching resume.
  lock.reset();

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkDownConfigureNotAllowed) {
  // Mock network could not be configured.
  ScopedCanConfigureNetwork can_configure_network(false, true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();
  WaitForAppLaunchNetworkTimeout();

  // Configure network link should not be visible.
  JsExpect("$('splash-config-network').hidden == true");

  // Network becomes online and app launch is resumed.
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkPortal) {
  // Mock network could be configured without the owner password.
  ScopedCanConfigureNetwork can_configure_network(true, false);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(SimulateNetworkPortalClosure());
  OobeScreenWaiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH)
      .WaitNoAssertCurrentScreen();
  WaitForAppLaunchNetworkTimeout();

  // Network error should show up automatically since this test does not
  // require owner auth to configure network.
  OobeScreenWaiter(OobeDisplay::SCREEN_ERROR_MESSAGE).Wait();

  ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppUserCancel) {
  // Make fake_cws_ return empty update response.
  set_test_app_version("");
  StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
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

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchInDiagnosticMode) {
  PrepareAppLaunch();
  SimulateNetworkOnline();

  LaunchApp(kTestKioskApp, true);

  content::WebContents* login_contents = GetLoginUI()->GetWebContents();

  bool new_kiosk_ui = KioskAppMenuHandler::EnableNewKioskUI();
  JsConditionWaiter(login_contents, new_kiosk_ui ?
      kCheckDiagnosticModeNewAPI : kCheckDiagnosticModeOldAPI).Wait();

  std::string diagnosticMode(new_kiosk_ui ?
      kCheckDiagnosticModeNewAPI : kCheckDiagnosticModeOldAPI);
  ASSERT_TRUE(content::ExecuteScript(
      login_contents,
      "(function() {"
         "var e = new Event('click');" +
         diagnosticMode + "."
             "okButton_.dispatchEvent(e);"
      "})();"));

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();

  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Start login screen after configuring auto launch app since the warning
  // is triggered when switching to login screen.
  wizard_controller->AdvanceToScreen(WizardController::kNetworkScreenName);
  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

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

IN_PROC_BROWSER_TEST_F(KioskTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();

  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Start login screen after configuring auto launch app since the warning
  // is triggered when switching to login screen.
  wizard_controller->AdvanceToScreen(WizardController::kNetworkScreenName);
  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

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

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableCancel) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
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
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
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
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableAbortedWithAutoEnrollment) {
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
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // The flow should be aborted due to auto enrollment enforcement.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  GetSigninScreenHandler()->set_kiosk_enable_flow_aborted_callback_for_test(
      runner->QuitClosure());
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableAfter2ndSigninScreen) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
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

  // Show signin screen again.
  chromeos::LoginDisplayHostImpl::default_host()->StartSignInScreen(
      LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();

  // Show kiosk enable screen again.
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // And it should show up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
}

class KioskUpdateTest : public KioskTest {
 public:
  KioskUpdateTest() {}
  virtual ~KioskUpdateTest() {}

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    KioskTest::SetUpOnMainThread();
  }

  void PreCacheApp(const std::string& app_id,
                   const std::string& version,
                   const std::string& crx_file) {
    set_test_app_id(app_id);
    set_test_app_version(version);
    set_test_crx_file(crx_file);

    KioskAppManager* manager = KioskAppManager::Get();
    AppDataLoadWaiter waiter(manager, app_id, version);
    ReloadKioskApps();
    waiter.Wait();
    EXPECT_TRUE(waiter.loaded());
    std::string cached_version;
    base::FilePath file_path;
    EXPECT_TRUE(manager->GetCachedCrx(app_id, &file_path, &cached_version));
    EXPECT_EQ(version, cached_version);
  }

  void UpdateExternalCache(const std::string& version,
                           const std::string& crx_file) {
    set_test_app_version(version);
    set_test_crx_file(crx_file);
    SetupTestAppUpdateCheck();

    KioskAppManager* manager = KioskAppManager::Get();
    AppDataLoadWaiter waiter(manager, test_app_id(), version);
    KioskAppManager::Get()->UpdateExternalCache();
    waiter.Wait();
    EXPECT_TRUE(waiter.loaded());
    std::string cached_version;
    base::FilePath file_path;
    EXPECT_TRUE(
        manager->GetCachedCrx(test_app_id(), &file_path, &cached_version));
    EXPECT_EQ(version, cached_version);
  }

  void PreCacheAndLaunchApp(const std::string& app_id,
                            const std::string& version,
                            const std::string& crx_file) {
    set_test_app_id(app_id);
    set_test_app_version(version);
    set_test_crx_file(crx_file);
    PrepareAppLaunch();
    SimulateNetworkOnline();
    LaunchApp(test_app_id(), false);
    WaitForAppLaunchSuccess();
    EXPECT_EQ(version, GetInstalledAppVersion().GetString());
  }

 private:
  class AppDataLoadWaiter : public KioskAppManagerObserver {
   public:
    AppDataLoadWaiter(KioskAppManager* manager,
                      const std::string& app_id,
                      const std::string& version)
        : runner_(NULL),
          manager_(manager),
          loaded_(false),
          quit_(false),
          app_id_(app_id),
          version_(version) {
      manager_->AddObserver(this);
    }

    virtual ~AppDataLoadWaiter() { manager_->RemoveObserver(this); }

    void Wait() {
      if (quit_)
        return;
      runner_ = new content::MessageLoopRunner;
      runner_->Run();
    }

    bool loaded() const { return loaded_; }

   private:
    // KioskAppManagerObserver overrides:
    virtual void OnKioskExtensionLoadedInCache(
        const std::string& app_id) OVERRIDE {
      std::string cached_version;
      base::FilePath file_path;
      if (!manager_->GetCachedCrx(app_id_, &file_path, &cached_version))
        return;
      if (version_ != cached_version)
        return;
      loaded_ = true;
      quit_ = true;
      if (runner_)
        runner_->Quit();
    }

    virtual void OnKioskExtensionDownloadFailed(
        const std::string& app_id) OVERRIDE {
      loaded_ = false;
      quit_ = true;
      if (runner_)
        runner_->Quit();
    }

    scoped_refptr<content::MessageLoopRunner> runner_;
    KioskAppManager* manager_;
    bool loaded_;
    bool quit_;
    std::string app_id_;
    std::string version_;

    DISALLOW_COPY_AND_ASSIGN(AppDataLoadWaiter);
  };

  DISALLOW_COPY_AND_ASSIGN(KioskUpdateTest);
};

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppNoNetwork) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp,
                       "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  StartUIForAppLaunch();
  SimulateNetworkOffline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_LaunchCachedOfflineEnabledAppNoNetwork) {
  PreCacheApp(kTestOfflineEnabledKioskApp,
              "1.0.0",
              std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchCachedOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  EXPECT_TRUE(
      KioskAppManager::Get()->HasCachedCrx(kTestOfflineEnabledKioskApp));
  StartUIForAppLaunch();
  SimulateNetworkOffline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

// Network offline, app v1.0 has run before, has cached v2.0 crx and v2.0 should
// be installed and launched during next launch.
IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       PRE_LaunchCachedNewVersionOfflineEnabledAppNoNetwork) {
  // Install and launch v1 app.
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp,
                       "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
  // Update cache for v2 app.
  UpdateExternalCache("2.0.0",
                      std::string(kTestOfflineEnabledKioskApp) + ".crx");
  // The installed app is still in v1.
  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest,
                       LaunchCachedNewVersionOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  EXPECT_TRUE(KioskAppManager::Get()->HasCachedCrx(test_app_id()));

  StartUIForAppLaunch();
  SimulateNetworkOffline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  // v2 app should have been installed.
  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppNoUpdate) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp,
                       "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  fake_cws()->SetNoUpdate(test_app_id());

  StartUIForAppLaunch();
  SimulateNetworkOnline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_LaunchOfflineEnabledAppHasUpdate) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp,
                       "1.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + "_v1.crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppHasUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  fake_cws()->SetUpdateCrx(
      test_app_id(), "ajoggoflpgplnnjkjamcmbepjdjdnpdp.crx", "2.0.0");

  StartUIForAppLaunch();
  SimulateNetworkOnline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_PermissionChange) {
  PreCacheAndLaunchApp(kTestOfflineEnabledKioskApp,
                       "2.0.0",
                       std::string(kTestOfflineEnabledKioskApp) + ".crx");
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PermissionChange) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_permission_change.crx");

  StartUIForAppLaunch();
  SimulateNetworkOnline();
  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PRE_PreserveLocalData) {
  // Installs v1 app and writes some local data.
  set_test_app_id(kTestLocalFsKioskApp);
  set_test_app_version("1.0.0");
  set_test_crx_file(test_app_id() + ".crx");

  ResultCatcher catcher;
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  WaitForAppLaunchSuccess();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, PreserveLocalData) {
  // Update existing v1 app installed in PRE_PreserveLocalData to v2
  // that reads and verifies the local data.
  set_test_app_id(kTestLocalFsKioskApp);
  set_test_app_version("2.0.0");
  set_test_crx_file(test_app_id() + "_v2_read_and_verify_data.crx");
  ResultCatcher catcher;
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

class KioskEnterpriseTest : public KioskTest {
 protected:
  KioskEnterpriseTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    device_policy_test_helper_.MarkAsEnterpriseOwned();
    device_policy_test_helper_.InstallOwnerKey();

    KioskTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    set_test_app_id(kTestEnterpriseKioskApp);
    set_test_app_version("1.0.0");
    set_test_crx_file(test_app_id() + ".crx");
    SetupTestAppUpdateCheck();

    KioskTest::SetUpOnMainThread();
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

    base::RunLoop run_loop;
    DBusThreadManager::Get()->GetSessionManagerClient()->StoreDevicePolicy(
        device_policy_test_helper_.device_policy()->GetBlob(),
        base::Bind(&KioskEnterpriseTest::StorePolicyCallback,
                   run_loop.QuitClosure()));
    run_loop.Run();

    DeviceSettingsService::Get()->Load();

    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    // This token satisfies the userinfo.email request from
    // DeviceOAuth2TokenService used in token validation.
    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kTestUserinfoToken;
    userinfo_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.email");
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kTestEnterpriseServiceAccountId;
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, userinfo_token_info);

    // The any-api access token for accessing the token minting endpoint.
    FakeGaia::AccessTokenInfo login_token_info;
    login_token_info.token = kTestLoginToken;
    login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
    login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, login_token_info);

    // This is the access token requested by the app via the identity API.
    FakeGaia::AccessTokenInfo access_token_info;
    access_token_info.token = kTestAccessToken;
    access_token_info.scopes.insert(kTestAppScope);
    access_token_info.audience = kTestClientId;
    access_token_info.email = kTestEnterpriseServiceAccountId;
    fake_gaia_->IssueOAuthToken(kTestLoginToken, access_token_info);

    DeviceOAuth2TokenService* token_service =
        DeviceOAuth2TokenServiceFactory::Get();
    token_service->SetAndSaveRefreshToken(
        kTestRefreshToken, DeviceOAuth2TokenService::StatusCallback());
    base::RunLoop().RunUntilIdle();
  }

  static void StorePolicyCallback(const base::Closure& callback, bool result) {
    ASSERT_TRUE(result);
    callback.Run();
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskEnterpriseTest);
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, EnterpriseKioskApp) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  // Wait for the Kiosk App configuration to reload, then launch the app.
  KioskAppManager::App app;
  content::WindowedNotificationObserver app_config_waiter(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      base::Bind(&KioskAppManager::GetApp,
                 base::Unretained(KioskAppManager::Get()),
                 kTestEnterpriseKioskApp, &app));
  FireKioskAppSettingsChanged();
  app_config_waiter.Wait();

  LaunchApp(kTestEnterpriseKioskApp, false);

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Wait for the window to appear.
  apps::AppWindow* window =
      AppWindowWaiter(
          apps::AppWindowRegistry::Get(ProfileManager::GetPrimaryUserProfile()),
          kTestEnterpriseKioskApp).Wait();
  ASSERT_TRUE(window);

  // Check whether the app can retrieve an OAuth2 access token.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      window->web_contents(),
      "chrome.identity.getAuthToken({ 'interactive': false }, function(token) {"
      "    window.domAutomationController.setAutomationId(0);"
      "    window.domAutomationController.send(token);"
      "});",
      &result));
  EXPECT_EQ(kTestAccessToken, result);

  // Verify that the session is not considered to be logged in with a GAIA
  // account.
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);
  EXPECT_FALSE(app_profile->GetPrefs()->HasPrefPath(
      prefs::kGoogleServicesUsername));

  // Terminate the app.
  window->GetBaseWindow()->Close();
  content::RunAllPendingInMessageLoop();
}

// Specialized test fixture for testing kiosk mode on the
// hidden WebUI initialization flow for slow hardware.
class KioskHiddenWebUITest : public KioskTest,
                             public ash::DesktopBackgroundControllerObserver {
 public:
  KioskHiddenWebUITest() : wallpaper_loaded_(false) {}

  // KioskTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    KioskTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableBootAnimation);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    KioskTest::SetUpOnMainThread();
    ash::Shell::GetInstance()->desktop_background_controller()
        ->AddObserver(this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ash::Shell::GetInstance()->desktop_background_controller()
        ->RemoveObserver(this);
    KioskTest::TearDownOnMainThread();
  }

  void WaitForWallpaper() {
    if (!wallpaper_loaded_) {
      runner_ = new content::MessageLoopRunner;
      runner_->Run();
    }
  }

  bool wallpaper_loaded() const { return wallpaper_loaded_; }

  // ash::DesktopBackgroundControllerObserver overrides:
  virtual void OnWallpaperDataChanged() OVERRIDE {
    wallpaper_loaded_ = true;
    if (runner_.get())
      runner_->Quit();
  }

  bool wallpaper_loaded_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(KioskHiddenWebUITest);
};

IN_PROC_BROWSER_TEST_F(KioskHiddenWebUITest, AutolaunchWarning) {
  // Add a device owner.
  FakeUserManager* user_manager = new FakeUserManager();
  user_manager->AddUser(kTestOwnerEmail);
  ScopedUserManagerEnabler enabler(user_manager);

  // Set kiosk app to autolaunch.
  EnableConsumerKioskMode();
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Start login screen after configuring auto launch app since the warning
  // is triggered when switching to login screen.
  wizard_controller->AdvanceToScreen(WizardController::kNetworkScreenName);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();

  // Wait for the wallpaper to load.
  WaitForWallpaper();
  EXPECT_TRUE(wallpaper_loaded());
}

}  // namespace chromeos

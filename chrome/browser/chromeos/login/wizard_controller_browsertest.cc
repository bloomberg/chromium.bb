// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_builder.h"
#include "base/prefs/testing_pref_store.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test_login_utils.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "grit/generated_resources.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::Exactly;
using ::testing::Return;

namespace chromeos {

namespace {
const char kUsername[] = "test_user@managedchrome.com";
const char kPassword[] = "test_password";

class PrefStoreStub : public TestingPrefStore {
 public:
  // TestingPrefStore overrides:
  virtual PrefReadError GetReadError() const OVERRIDE {
    return PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
  }

  virtual bool IsInitializationComplete() const OVERRIDE {
    return true;
  }

 private:
  virtual ~PrefStoreStub() {}
};

}  // namespace

using ::testing::_;

template <class T, class H>
class MockOutShowHide : public T {
 public:
  template <class P> explicit  MockOutShowHide(P p) : T(p) {}
  template <class P> MockOutShowHide(P p, H* actor)
      : T(p, actor), actor_(actor) {}

  H* actor() const { return actor_.get(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

 private:
  scoped_ptr<H> actor_;
};

#define MOCK(mock_var, screen_name, mocked_class, actor_class)                 \
  mock_var = new MockOutShowHide<mocked_class, actor_class>(                   \
      WizardController::default_controller(), new actor_class);                \
  WizardController::default_controller()->screen_name.reset(mock_var);         \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                     \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

class WizardControllerTest : public WizardInProcessBrowserTest {
 protected:
  WizardControllerTest() : WizardInProcessBrowserTest(
      WizardController::kTestNoScreenName) {}
  virtual ~WizardControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kNetworkScreenName);

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring en_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  LanguageSwitchMenu::SwitchLanguage("fr");
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring fr_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  EXPECT_NE(en_str, fr_str);

  LanguageSwitchMenu::SwitchLanguage("ar");
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(base::i18n::IsRTL());
  const std::wstring ar_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  EXPECT_NE(fr_str, ar_str);
}

class WizardControllerFlowTest : public WizardControllerTest {
 protected:
  WizardControllerFlowTest() {}
  // Overriden from InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    MOCK(mock_network_screen_, network_screen_,
         MockNetworkScreen, MockNetworkScreenActor);
    MOCK(mock_update_screen_, update_screen_,
         MockUpdateScreen, MockUpdateScreenActor);
    MOCK(mock_eula_screen_, eula_screen_, MockEulaScreen, MockEulaScreenActor);
    MOCK(mock_enrollment_screen_, enrollment_screen_,
         MockEnrollmentScreen, MockEnrollmentScreenActor);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, WizardController::default_controller()->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    WizardController::default_controller()->AdvanceToScreen(
        WizardController::kNetworkScreenName);
  }

  void OnExit(ScreenObserver::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkScreenActor>*
      mock_network_screen_;
  MockOutShowHide<MockUpdateScreen, MockUpdateScreenActor>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaScreenActor>* mock_eula_screen_;
  MockOutShowHide<MockEnrollmentScreen,
      MockEnrollmentScreenActor>* mock_enrollment_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ("ethernet,wifi,cellular",
            NetworkHandler::Get()->network_state_handler()
            ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowErrorUpdate) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenObserver::UPDATE_ERROR_UPDATING);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            false,  // is_auto_enrollment
                            true,   // can_exit_enrollment
                            ""))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetEnrollmentScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ("ethernet,wifi,cellular",
            NetworkHandler::Get()->network_state_handler()
            ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenObserver::EULA_BACK);

  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            false,  // is_auto_enrollment
                            true,   // can_exit_enrollment
                            ""))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kEnrollmentScreenName);
  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowAutoEnrollmentCompleted) {
  WizardController::default_controller()->SkipPostLoginScreensForTesting();
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);

  LoginUtils::Set(new TestLoginUtils(kUsername, kPassword));
  MockConsumer mock_consumer;

  // Must have a pending signin to resume after auto-enrollment:
  LoginDisplayHostImpl::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->DoAutoEnrollment();
  ExistingUserController::current_controller()->set_login_status_consumer(
      &mock_consumer);
  // This calls StartWizard, destroying the current controller() and its mocks;
  // don't set expectations on those objects.
  ExistingUserController::current_controller()->CompleteLogin(
      UserContext(kUsername, kPassword, ""));
  // Run the tasks posted to complete the login:
  base::MessageLoop::current()->RunUntilIdle();

  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  // This is the main expectation: after auto-enrollment, login is resumed.
  EXPECT_CALL(mock_consumer, OnLoginSuccess(_)).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED);
  // Prevent browser launch when the profile is prepared:
  browser_shutdown::SetTryingToQuit(true);
  // Run the tasks posted to complete the login:
  base::MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowResetScreen) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  LoginDisplayHostImpl::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->OnStartDeviceReset();

  ResetScreen* screen =
      WizardController::default_controller()->GetResetScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());

  // After reset screen is canceled, it returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(ScreenObserver::RESET_CANCELED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  LoginDisplayHostImpl::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->ShowWrongHWIDScreen();

  WrongHWIDScreen* screen =
      WizardController::default_controller()->GetWrongHWIDScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(ScreenObserver::WRONG_HWID_WARNING_SKIPPED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 protected:
  WizardControllerBrokenLocalStateTest()
      : fake_session_manager_client_(NULL) {
  }

  virtual ~WizardControllerBrokenLocalStateTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();

    FakeDBusThreadManager* fake_dbus_thread_manager =
        new FakeDBusThreadManager();
    fake_session_manager_client_ =
        fake_dbus_thread_manager->fake_session_manager_client();
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    PrefServiceBuilder builder;
    local_state_.reset(builder
                       .WithUserPrefs(new PrefStoreStub())
                       .Create(new PrefRegistrySimple()));
    WizardController::set_local_state_for_testing(local_state_.get());

    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    WizardControllerTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  ErrorScreen* GetErrorScreen() {
    return ((ScreenObserver*) WizardController::default_controller())->
        GetErrorScreen();
  }

  content::WebContents* GetWebContents() {
    LoginDisplayHostImpl* host = static_cast<LoginDisplayHostImpl*>(
        LoginDisplayHostImpl::default_host());
    if (!host)
      return NULL;
    WebUILoginView* webui_login_view = host->GetWebUILoginView();
    if (!webui_login_view)
      return NULL;
    return webui_login_view->GetWebContents();
  }

  void WaitUntilJSIsReady() {
    LoginDisplayHostImpl* host = static_cast<LoginDisplayHostImpl*>(
        LoginDisplayHostImpl::default_host());
    if (!host)
      return;
    chromeos::OobeUI* oobe_ui = host->GetOobeUI();
    if (!oobe_ui)
      return;
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready)
      run_loop.Run();
  }

  bool JSExecuteBooleanExpression(const std::string& expression) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    return result;
  }

  FakeSessionManagerClient* fake_session_manager_client() const {
    return fake_session_manager_client_;
  }

 private:
  scoped_ptr<PrefService> local_state_;
  FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerBrokenLocalStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerBrokenLocalStateTest,
                       LocalStateCorrupted) {
  // Checks that after wizard controller initialization error screen
  // in the proper state is displayed.
  ASSERT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  ASSERT_EQ(ErrorScreen::UI_STATE_LOCAL_STATE_ERROR,
            GetErrorScreen()->GetUIState());

  WaitUntilJSIsReady();

  // Checks visibility of the error message and powerwash button.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('error-message').hidden"));
  ASSERT_TRUE(JSExecuteBooleanExpression(
      "$('error-message').classList.contains('ui-state-local-state-error')"));
  ASSERT_TRUE(JSExecuteBooleanExpression("$('progress-dots').hidden"));
  ASSERT_TRUE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  // Emulates user click on the "Restart and Powerwash" button.
  ASSERT_EQ(0, fake_session_manager_client()->start_device_wipe_call_count());
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "$('error-message-restart-and-powerwash-button').click();"));
  ASSERT_EQ(1, fake_session_manager_client()->start_device_wipe_call_count());
}

class WizardControllerProxyAuthOnSigninTest : public WizardControllerTest {
 protected:
  WizardControllerProxyAuthOnSigninTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      net::SpawnedTestServer::kLocalhost,
                      base::FilePath()) {
  }
  virtual ~WizardControllerProxyAuthOnSigninTest() {}

  // Overridden from WizardControllerTest:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(proxy_server_.Start());
    WizardControllerTest::SetUp();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WizardControllerTest::SetUpOnMainThread();
    WizardController::default_controller()->AdvanceToScreen(
        WizardController::kNetworkScreenName);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

  net::SpawnedTestServer& proxy_server() { return proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerProxyAuthOnSigninTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerProxyAuthOnSigninTest,
                       ProxyAuthDialogOnSigninScreen) {
  content::WindowedNotificationObserver auth_needed_waiter(
      chrome::NOTIFICATION_AUTH_NEEDED,
      content::NotificationService::AllSources());

  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  LoginDisplayHostImpl::default_host()->StartSignInScreen();
  auth_needed_waiter.Wait();
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerKioskFlowTest() {}

  // Overridden from InProcessBrowserTest:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
                    "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerKioskFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowKioskForcedEnrollment) {
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            false,  // is_auto_enrollment
                            false,  // can_exit_enrollment
                            ""))
      .Times(1);

  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

// TODO(dzhioev): Add test emaulating device with wrong HWID.

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

COMPILE_ASSERT(ScreenObserver::EXIT_CODES_COUNT == 18,
               add_tests_for_new_control_flow_you_just_introduced);

}  // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/geolocation/simple_geolocation_provider.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/test_login_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/timezone/timezone_request.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/mock_auth_status_consumer.h"
#include "chromeos/login/auth/mock_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/timezone_settings.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

const char kUsername[] = "test_user@managedchrome.com";
const char kPassword[] = "test_password";

const char kGeolocationResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";

// Timezone should not match kGeolocationResponseBody to check that exactly
// this value will be used.
const char kTimezoneResponseBody[] =
    "{\n"
    "    \"dstOffset\" : 0.0,\n"
    "    \"rawOffset\" : -32400.0,\n"
    "    \"status\" : \"OK\",\n"
    "    \"timeZoneId\" : \"America/Anchorage\",\n"
    "    \"timeZoneName\" : \"Pacific Standard Time\"\n"
    "}";

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

struct SwitchLanguageTestData {
  SwitchLanguageTestData() : success(false), done(false) {}

  std::string requested_locale;
  std::string loaded_locale;
  bool success;
  bool done;
};

void OnLocaleSwitched(SwitchLanguageTestData* self,
                      const std::string& locale,
                      const std::string& loaded_locale,
                      const bool success) {
  self->requested_locale = locale;
  self->loaded_locale = loaded_locale;
  self->success = success;
  self->done = true;
}

void RunSwitchLanguageTest(const std::string& locale,
                                  const std::string& expected_locale,
                                  const bool expect_success) {
  SwitchLanguageTestData data;
  scoped_ptr<locale_util::SwitchLanguageCallback> callback(
      new locale_util::SwitchLanguageCallback(
          base::Bind(&OnLocaleSwitched, base::Unretained(&data))));
  locale_util::SwitchLanguage(locale, true, false, callback.Pass());

  // Token writing moves control to BlockingPool and back.
  base::RunLoop().RunUntilIdle();
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(data.done, true);
  EXPECT_EQ(data.requested_locale, locale);
  EXPECT_EQ(data.loaded_locale, expected_locale);
  EXPECT_EQ(data.success, expect_success);
}

void SetUpCrasAndEnableChromeVox(int volume_percent, bool mute_on) {
  AccessibilityManager* a11y = AccessibilityManager::Get();
  CrasAudioHandler* cras = CrasAudioHandler::Get();

  // Audio output is at |volume_percent| and |mute_on|. Spoken feedback
  // is disabled.
  cras->SetOutputVolumePercent(volume_percent);
  cras->SetOutputMute(mute_on);
  a11y->EnableSpokenFeedback(false, ash::A11Y_NOTIFICATION_NONE);

  // Spoken feedback is enabled.
  a11y->EnableSpokenFeedback(true, ash::A11Y_NOTIFICATION_NONE);
  base::RunLoop().RunUntilIdle();
}

void QuitLoopOnAutoEnrollmentProgress(
    policy::AutoEnrollmentState expected_state,
    base::RunLoop* loop,
    policy::AutoEnrollmentState actual_state) {
  if (expected_state == actual_state)
    loop->Quit();
}

void WaitForAutoEnrollmentState(policy::AutoEnrollmentState state) {
  base::RunLoop loop;
  AutoEnrollmentController* auto_enrollment_controller =
      LoginDisplayHostImpl::default_host()->GetAutoEnrollmentController();
  scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
      progress_subscription(
          auto_enrollment_controller->RegisterProgressCallback(
              base::Bind(&QuitLoopOnAutoEnrollmentProgress, state, &loop)));
  loop.Run();
}

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

  virtual void SetUpOnMainThread() OVERRIDE {
    AccessibilityManager::Get()->
        SetProfileForTest(ProfileHelper::GetSigninProfile());
    WizardInProcessBrowserTest::SetUpOnMainThread();
  }

  ErrorScreen* GetErrorScreen() {
    return static_cast<ScreenObserver*>(WizardController::default_controller())
        ->GetErrorScreen();
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
  const base::string16 en_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

  RunSwitchLanguageTest("fr", "fr", true);
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const base::string16 fr_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

  EXPECT_NE(en_str, fr_str);

  RunSwitchLanguageTest("ar", "ar", true);
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(base::i18n::IsRTL());
  const base::string16 ar_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

  EXPECT_NE(fr_str, ar_str);
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsChangedForChromeVox) {
  SetUpCrasAndEnableChromeVox(75 /* volume_percent */, true /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(WizardController::kMinAudibleOutputVolumePercent,
            cras->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsUnchangedForChromeVox) {
  SetUpCrasAndEnableChromeVox(75 /* volume_percent */, false /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(75, cras->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsAdjustedForChromeVox) {
  SetUpCrasAndEnableChromeVox(5 /* volume_percent */, false /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(WizardController::kMinAudibleOutputVolumePercent,
            cras->GetOutputVolumePercent());
}

class WizardControllerTestURLFetcherFactory
    : public net::TestURLFetcherFactory {
 public:
  virtual net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) OVERRIDE {
    if (StartsWithASCII(
            url.spec(),
            SimpleGeolocationProvider::DefaultGeolocationProviderURL().spec(),
            true)) {
      return new net::FakeURLFetcher(url,
                                     d,
                                     std::string(kGeolocationResponseBody),
                                     net::HTTP_OK,
                                     net::URLRequestStatus::SUCCESS);
    }
    if (StartsWithASCII(url.spec(),
                        chromeos::DefaultTimezoneProviderURL().spec(),
                        true)) {
      return new net::FakeURLFetcher(url,
                                     d,
                                     std::string(kTimezoneResponseBody),
                                     net::HTTP_OK,
                                     net::URLRequestStatus::SUCCESS);
    }
    return net::TestURLFetcherFactory::CreateURLFetcher(
        id, url, request_type, d);
  }
  virtual ~WizardControllerTestURLFetcherFactory() {}
};

class TimeZoneTestRunner {
 public:
  void OnResolved() { loop_.Quit(); }
  void Run() { loop_.Run(); }

 private:
  base::RunLoop loop_;
};

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
    MOCK(mock_auto_enrollment_check_screen_, auto_enrollment_check_screen_,
         MockAutoEnrollmentCheckScreen, MockAutoEnrollmentCheckScreenActor);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, WizardController::default_controller()->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    WizardController::default_controller()->AdvanceToScreen(
        WizardController::kNetworkScreenName);
  }

  virtual void TearDown() {
    if (fallback_fetcher_factory_) {
      fetcher_factory_.reset();
      net::URLFetcherImpl::set_factory(fallback_fetcher_factory_.get());
      fallback_fetcher_factory_.reset();
    }
  }

  void InitTimezoneResolver() {
    fallback_fetcher_factory_.reset(new WizardControllerTestURLFetcherFactory);
    net::URLFetcherImpl::set_factory(NULL);
    fetcher_factory_.reset(
        new net::FakeURLFetcherFactory(fallback_fetcher_factory_.get()));

    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(network_portal_detector_);

    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;

    // Default detworks happens to be usually "eth1" in tests.
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();

    network_portal_detector_->SetDefaultNetworkForTesting(
        default_network->guid());
    network_portal_detector_->SetDetectionResultsForTesting(
        default_network->guid(),
        online_state);
  }

  void OnExit(ScreenObserver::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
  }

  chromeos::SimpleGeolocationProvider* GetGeolocationProvider() {
    return WizardController::default_controller()->geolocation_provider_.get();
  }

  void WaitUntilTimezoneResolved() {
    scoped_ptr<TimeZoneTestRunner> runner(new TimeZoneTestRunner);
    if (!WizardController::default_controller()
             ->SetOnTimeZoneResolvedForTesting(
                 base::Bind(&TimeZoneTestRunner::OnResolved,
                            base::Unretained(runner.get()))))
      return;

    runner->Run();
  }

  void ResetAutoEnrollmentCheckScreen() {
    WizardController::default_controller()->
        auto_enrollment_check_screen_.reset();
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkScreenActor>*
      mock_network_screen_;
  MockOutShowHide<MockUpdateScreen, MockUpdateScreenActor>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaScreenActor>* mock_eula_screen_;
  MockOutShowHide<MockEnrollmentScreen,
      MockEnrollmentScreenActor>* mock_enrollment_screen_;
  MockOutShowHide<MockAutoEnrollmentCheckScreen,
      MockAutoEnrollmentCheckScreenActor>* mock_auto_enrollment_check_screen_;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  // Use a test factory as a fallback so we don't have to deal with other
  // requests.
  scoped_ptr<WizardControllerTestURLFetcherFactory> fallback_fetcher_factory_;
  scoped_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  WaitUntilJSIsReady();

  // Check visibility of the header bar.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());

  // Header bar should still be visible.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  // Enable TimeZone resolve
  InitTimezoneResolver();
  OnExit(ScreenObserver::EULA_ACCEPTED);
  EXPECT_TRUE(GetGeolocationProvider());
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_EQ(
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen(),
      WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ("ethernet,wifi,cellular",
            NetworkHandler::Get()->network_state_handler()
            ->GetCheckPortalListForTest());

  WaitUntilTimezoneResolved();
  EXPECT_EQ("America/Anchorage",
            base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                                  ->GetCurrentTimezoneID()));
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
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_ERROR_UPDATING);

  EXPECT_EQ(
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen(),
      WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

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
                            EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL,
                            ""))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen(),
      WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
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
                            EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL,
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

  UserContext user_context(kUsername);
  user_context.SetKey(Key(kPassword));
  user_context.SetUserIDHash(user_context.GetUserID());
  LoginUtils::Set(new TestLoginUtils(user_context));
  MockAuthStatusConsumer mock_consumer;

  // Must have a pending signin to resume after auto-enrollment:
  LoginDisplayHostImpl::default_host()->StartSignInScreen(LoginScreenContext());
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->DoAutoEnrollment();
  ExistingUserController::current_controller()->set_login_status_consumer(
      &mock_consumer);
  // This calls StartWizard, destroying the current controller() and its mocks;
  // don't set expectations on those objects.
  ExistingUserController::current_controller()->CompleteLogin(user_context);
  // Run the tasks posted to complete the login:
  base::MessageLoop::current()->RunUntilIdle();

  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  // This is the main expectation: after auto-enrollment, login is resumed.
  EXPECT_CALL(mock_consumer, OnAuthSuccess(_)).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED);
  // Prevent browser launch when the profile is prepared:
  browser_shutdown::SetTryingToQuit(true);
  // Run the tasks posted to complete the login:
  base::MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  LoginDisplayHostImpl::default_host()->StartSignInScreen(LoginScreenContext());
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

class WizardControllerEnrollmentFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerEnrollmentFlowTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WizardControllerFlowTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        chromeos::AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "1");
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentModulusLimit, "2");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerEnrollmentFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnrollmentFlowTest,
                       ControlFlowForcedReEnrollment) {
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
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  AutoEnrollmentCheckScreen* screen =
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen();
  EXPECT_EQ(screen,
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  screen->Start();
  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentScreenActor::ENROLLMENT_MODE_FORCED,
                            "")).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  EXPECT_EQ(WizardController::default_controller()->GetEnrollmentScreen(),
            WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
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
    fake_dbus_thread_manager->SetFakeClients();
    fake_session_manager_client_ = new FakeSessionManagerClient;
    fake_dbus_thread_manager->SetSessionManagerClient(
        scoped_ptr<SessionManagerClient>(fake_session_manager_client_));
    DBusThreadManager::SetInstanceForTesting(fake_dbus_thread_manager);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    base::PrefServiceFactory factory;
    factory.set_user_prefs(make_scoped_refptr(new PrefStoreStub()));
    local_state_ = factory.Create(new PrefRegistrySimple()).Pass();
    WizardController::set_local_state_for_testing(local_state_.get());

    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    WizardControllerTest::TearDownInProcessBrowserTestFixture();
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

  LoginDisplayHostImpl::default_host()->StartSignInScreen(LoginScreenContext());
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
                            EnrollmentScreenActor::ENROLLMENT_MODE_FORCED,
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
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_EQ(
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen(),
      WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);


  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}


IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBack) {
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentScreenActor::ENROLLMENT_MODE_FORCED,
                            ""))
      .Times(1);

  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
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
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_EQ(
      WizardController::default_controller()->GetAutoEnrollmentCheckScreen(),
      WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  EnrollmentScreen* screen =
      WizardController::default_controller()->GetEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_BACK);

  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerOobeResumeTest : public WizardControllerTest {
 protected:
  WizardControllerOobeResumeTest() {}
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
    MOCK(mock_enrollment_screen_, enrollment_screen_,
         MockEnrollmentScreen, MockEnrollmentScreenActor);
  }

  void OnExit(ScreenObserver::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
  }

  std::string GetFirstScreenName() {
    return WizardController::default_controller()->first_screen_name();
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkScreenActor>*
      mock_network_screen_;
  MockOutShowHide<MockEnrollmentScreen,
      MockEnrollmentScreenActor>* mock_enrollment_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeResumeTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       PRE_ControlFlowResumeInterruptedOobe) {
  // Switch to the initial screen.
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kNetworkScreenName);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL,
                            ""))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kEnrollmentScreenName);
  EXPECT_EQ(WizardController::default_controller()->GetEnrollmentScreen(),
            WizardController::default_controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       ControlFlowResumeInterruptedOobe) {
  EXPECT_EQ(WizardController::kEnrollmentScreenName, GetFirstScreenName());
}

// TODO(dzhioev): Add test emaulating device with wrong HWID.

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

// TODO(merkulova): Add tests for bluetooth HID detection screen variations when
// UI and logic is ready. http://crbug.com/127016

// TODO(dzhioev): Add tests for controller/host pairing flow.
// http://crbug.com/375191

COMPILE_ASSERT(ScreenObserver::EXIT_CODES_COUNT == 23,
               add_tests_for_new_control_flow_you_just_introduced);

}  // namespace chromeos

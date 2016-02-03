// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/mock_enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/timezone/timezone_request.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
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
using ::testing::Mock;
using ::testing::Return;

namespace chromeos {

namespace {

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

const char kDisabledMessage[] = "This device has been disabled.";

// Matches on the mode parameter of an EnrollmentConfig object.
MATCHER_P(EnrollmentModeMatches, mode, "") {
  return arg.mode == mode;
}

class PrefStoreStub : public TestingPrefStore {
 public:
  // TestingPrefStore overrides:
  PrefReadError GetReadError() const override {
    return PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
  }

  bool IsInitializationComplete() const override { return true; }

 private:
  ~PrefStoreStub() override {}
};

struct SwitchLanguageTestData {
  SwitchLanguageTestData() : result("", "", false), done(false) {}

  locale_util::LanguageSwitchResult result;
  bool done;
};

void OnLocaleSwitched(SwitchLanguageTestData* self,
                      const locale_util::LanguageSwitchResult& result) {
  self->result = result;
  self->done = true;
}

void RunSwitchLanguageTest(const std::string& locale,
                                  const std::string& expected_locale,
                                  const bool expect_success) {
  SwitchLanguageTestData data;
  locale_util::SwitchLanguageCallback callback(
      base::Bind(&OnLocaleSwitched, base::Unretained(&data)));
  locale_util::SwitchLanguage(locale, true, false, callback,
                              ProfileManager::GetActiveUserProfile());

  // Token writing moves control to BlockingPool and back.
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_EQ(data.done, true);
  EXPECT_EQ(data.result.requested_locale, locale);
  EXPECT_EQ(data.result.loaded_locale, expected_locale);
  EXPECT_EQ(data.result.success, expect_success);
}

void SetUpCrasAndEnableChromeVox(int volume_percent, bool mute_on) {
  AccessibilityManager* a11y = AccessibilityManager::Get();
  CrasAudioHandler* cras = CrasAudioHandler::Get();

  // Audio output is at |volume_percent| and |mute_on|. Spoken feedback
  // is disabled.
  cras->SetOutputVolumePercent(volume_percent);
  cras->SetOutputMute(mute_on);
  a11y->EnableSpokenFeedback(false, ui::A11Y_NOTIFICATION_NONE);

  // Spoken feedback is enabled.
  a11y->EnableSpokenFeedback(true, ui::A11Y_NOTIFICATION_NONE);
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
      LoginDisplayHost::default_host()->GetAutoEnrollmentController();
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
  template <class P, class Q>
  MockOutShowHide(P p, Q q, H* actor)
      : T(p, q, actor), actor_(actor) {}

  H* actor() const { return actor_.get(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void RealShow() {
    T::Show();
  }

  void RealHide() {
    T::Hide();
  }

 private:
  scoped_ptr<H> actor_;
};

#define MOCK(mock_var, screen_name, mocked_class, actor_class)               \
  mock_var = new MockOutShowHide<mocked_class, actor_class>(                 \
      WizardController::default_controller(), new actor_class);              \
  WizardController::default_controller()                                     \
      ->screens_[WizardController::screen_name] = make_linked_ptr(mock_var); \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                   \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

#define MOCK_WITH_DELEGATE(mock_var, screen_name, mocked_class, actor_class) \
  mock_var = new MockOutShowHide<mocked_class, actor_class>(                 \
      WizardController::default_controller(),                                \
      WizardController::default_controller(),                                \
      new actor_class);                                                      \
  WizardController::default_controller()                                     \
      ->screens_[WizardController::screen_name] = make_linked_ptr(mock_var); \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                   \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

class WizardControllerTest : public WizardInProcessBrowserTest {
 protected:
  WizardControllerTest() : WizardInProcessBrowserTest(
      WizardController::kTestNoScreenName) {}
  ~WizardControllerTest() override {}

  void SetUpOnMainThread() override {
    AccessibilityManager::Get()->
        SetProfileForTest(ProfileHelper::GetSigninProfile());
    WizardInProcessBrowserTest::SetUpOnMainThread();
  }

  ErrorScreen* GetErrorScreen() {
    return static_cast<BaseScreenDelegate*>(
               WizardController::default_controller())->GetErrorScreen();
  }

  OobeUI* GetOobeUI() { return LoginDisplayHost::default_host()->GetOobeUI(); }

  content::WebContents* GetWebContents() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host)
      return NULL;
    WebUILoginView* webui_login_view = host->GetWebUILoginView();
    if (!webui_login_view)
      return NULL;
    return webui_login_view->GetWebContents();
  }

  void WaitUntilJSIsReady() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
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

  bool JSExecute(const std::string& expression) {
    return content::ExecuteScript(
        GetWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));");
  }

  bool JSExecuteBooleanExpression(const std::string& expression) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    return result;
  }

  void CheckCurrentScreen(const std::string& screen_name) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen_name),
              WizardController::default_controller()->current_screen());
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
  scoped_ptr<net::URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    if (base::StartsWith(
            url.spec(),
            SimpleGeolocationProvider::DefaultGeolocationProviderURL().spec(),
            base::CompareCase::SENSITIVE)) {
      return scoped_ptr<net::URLFetcher>(new net::FakeURLFetcher(
          url, d, std::string(kGeolocationResponseBody), net::HTTP_OK,
          net::URLRequestStatus::SUCCESS));
    }
    if (base::StartsWith(url.spec(),
                         chromeos::DefaultTimezoneProviderURL().spec(),
                         base::CompareCase::SENSITIVE)) {
      return scoped_ptr<net::URLFetcher>(new net::FakeURLFetcher(
          url, d, std::string(kTimezoneResponseBody), net::HTTP_OK,
          net::URLRequestStatus::SUCCESS));
    }
    return net::TestURLFetcherFactory::CreateURLFetcher(
        id, url, request_type, d);
  }
  ~WizardControllerTestURLFetcherFactory() override {}
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
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    mock_network_screen_.reset(new MockNetworkScreen(
        WizardController::default_controller(),
        WizardController::default_controller(), GetOobeUI()->GetNetworkView()));
    mock_network_screen_->Initialize(nullptr /* context */);
    WizardController::default_controller()
        ->screens_[WizardController::kNetworkScreenName] = mock_network_screen_;
    EXPECT_CALL(*mock_network_screen_, Show()).Times(0);
    EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);

    MOCK(mock_update_screen_, kUpdateScreenName, MockUpdateScreen,
         MockUpdateView);
    MOCK_WITH_DELEGATE(mock_eula_screen_, kEulaScreenName, MockEulaScreen,
                       MockEulaView);
    MOCK(mock_enrollment_screen_,
         kEnrollmentScreenName,
         MockEnrollmentScreen,
         MockEnrollmentScreenActor);
    MOCK(mock_auto_enrollment_check_screen_,
         kAutoEnrollmentCheckScreenName,
         MockAutoEnrollmentCheckScreen,
         MockAutoEnrollmentCheckScreenActor);
    MOCK(mock_wrong_hwid_screen_, kWrongHWIDScreenName, MockWrongHWIDScreen,
         MockWrongHWIDScreenActor);
    MOCK(mock_enable_debugging_screen_,
         kEnableDebuggingScreenName,
         MockEnableDebuggingScreen,
         MockEnableDebuggingScreenActor);
    device_disabled_screen_actor_.reset(new MockDeviceDisabledScreenActor);
    wizard_controller->screens_[WizardController::kDeviceDisabledScreenName] =
        make_linked_ptr(new DeviceDisabledScreen(
            wizard_controller,
            device_disabled_screen_actor_.get()));
    EXPECT_CALL(*device_disabled_screen_actor_, Show()).Times(0);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, wizard_controller->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    wizard_controller->AdvanceToScreen(WizardController::kNetworkScreenName);
  }

  void TearDownOnMainThread() override {
    mock_network_screen_.reset();
    device_disabled_screen_actor_.reset();
    WizardControllerTest::TearDownOnMainThread();
  }

  void TearDown() override {
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
    network_portal_detector::InitializeForTesting(network_portal_detector_);

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

  void OnExit(BaseScreen& screen, BaseScreenDelegate::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(screen, exit_code,
                                                   nullptr /* context */);
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
    WizardController::default_controller()->screens_.erase(
        WizardController::kAutoEnrollmentCheckScreenName);
  }

  linked_ptr<MockNetworkScreen> mock_network_screen_;
  MockOutShowHide<MockUpdateScreen, MockUpdateView>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaView>* mock_eula_screen_;
  MockOutShowHide<MockEnrollmentScreen,
      MockEnrollmentScreenActor>* mock_enrollment_screen_;
  MockOutShowHide<MockAutoEnrollmentCheckScreen,
      MockAutoEnrollmentCheckScreenActor>* mock_auto_enrollment_check_screen_;
  MockOutShowHide<MockWrongHWIDScreen, MockWrongHWIDScreenActor>*
      mock_wrong_hwid_screen_;
  MockOutShowHide<MockEnableDebuggingScreen,
      MockEnableDebuggingScreenActor>* mock_enable_debugging_screen_;
  scoped_ptr<MockDeviceDisabledScreenActor> device_disabled_screen_actor_;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  // Use a test factory as a fallback so we don't have to deal with other
  // requests.
  scoped_ptr<WizardControllerTestURLFetcherFactory> fallback_fetcher_factory_;
  scoped_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);

  WaitUntilJSIsReady();

  // Check visibility of the header bar.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);

  // Header bar should still be visible.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  // Enable TimeZone resolve
  InitTimezoneResolver();
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);
  EXPECT_TRUE(GetGeolocationProvider());

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

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
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_ERROR_UPDATING);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
  EXPECT_EQ("ethernet,wifi,cellular",
            NetworkHandler::Get()->network_state_handler()
            ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_BACK);

  CheckCurrentScreen(WizardController::kNetworkScreenName);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kEnrollmentScreenName);
  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
  OnExit(*mock_enrollment_screen_,
         BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->ShowWrongHWIDScreen();

  CheckCurrentScreen(WizardController::kWrongHWIDScreenName);

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(*mock_wrong_hwid_screen_,
         BaseScreenDelegate::WRONG_HWID_WARNING_SKIPPED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

class WizardControllerDeviceStateTest : public WizardControllerFlowTest {
 protected:
  WizardControllerDeviceStateTest()
      : install_attributes_(std::string(),
                            std::string(),
                            std::string(),
                            policy::DEVICE_MODE_NOT_SET) {
    fake_statistics_provider_.SetMachineStatistic("serial_number", "test");
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  "2000-01");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        chromeos::AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "1");
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentModulusLimit, "2");
  }

  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

 private:
  policy::ScopedStubEnterpriseInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerDeviceStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowForcedReEnrollment) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

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
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
  OnExit(*mock_enrollment_screen_,
         BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowNoForcedReEnrollmentOnFirstBoot) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  EXPECT_NE(
      policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
      LoginDisplayHost::default_host()->GetAutoEnrollmentController()->state());

  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(
      policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
      LoginDisplayHost::default_host()->GetAutoEnrollmentController()->state());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowDeviceDisabled) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if device state could not be retrieved.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeDisabled);
  device_state.SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*device_disabled_screen_actor_,
              UpdateMessage(kDisabledMessage)).Times(1);
  EXPECT_CALL(*device_disabled_screen_actor_, Show()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure the device disabled screen is shown.
  CheckCurrentScreen(WizardController::kDeviceDisabledScreenName);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 protected:
  WizardControllerBrokenLocalStateTest()
      : fake_session_manager_client_(NULL) {
  }

  ~WizardControllerBrokenLocalStateTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();

    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        scoped_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  void SetUpOnMainThread() override {
    base::PrefServiceFactory factory;
    factory.set_user_prefs(make_scoped_refptr(new PrefStoreStub()));
    local_state_ = factory.Create(new PrefRegistrySimple());
    WizardController::set_local_state_for_testing(local_state_.get());

    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;
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
  ASSERT_EQ(NetworkError::UI_STATE_LOCAL_STATE_ERROR,
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
  ~WizardControllerProxyAuthOnSigninTest() override {}

  // Overridden from WizardControllerTest:
  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    WizardControllerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
    WizardController::default_controller()->AdvanceToScreen(
        WizardController::kNetworkScreenName);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

  net::SpawnedTestServer& proxy_server() { return proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerProxyAuthOnSigninTest);
};

// Disabled, see https://crbug.com/504928.
IN_PROC_BROWSER_TEST_F(WizardControllerProxyAuthOnSigninTest,
                       DISABLED_ProxyAuthDialogOnSigninScreen) {
  content::WindowedNotificationObserver auth_needed_waiter(
      chrome::NOTIFICATION_AUTH_NEEDED,
      content::NotificationService::AllSources());

  CheckCurrentScreen(WizardController::kNetworkScreenName);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  auth_needed_waiter.Wait();
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerKioskFlowTest() {}

  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
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
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
  OnExit(*mock_enrollment_screen_,
         BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBack) {
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);

  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, BaseScreenDelegate::NETWORK_CONNECTED);

  CheckCurrentScreen(WizardController::kEulaScreenName);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, BaseScreenDelegate::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kUpdateScreenName);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, BaseScreenDelegate::UPDATE_INSTALLED);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         BaseScreenDelegate::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_enrollment_screen_,
         BaseScreenDelegate::ENTERPRISE_ENROLLMENT_BACK);

  CheckCurrentScreen(WizardController::kAutoEnrollmentCheckScreenName);
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}


class WizardControllerEnableDebuggingTest : public WizardControllerFlowTest {
 protected:
  WizardControllerEnableDebuggingTest() {}

  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerEnableDebuggingTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableDebuggingTest,
                       ShowAndCancelEnableDebugging) {
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enable_debugging_screen_, Show()).Times(1);

  ASSERT_TRUE(JSExecute("$('connect-debugging-features-link').click()"));

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kEnableDebuggingScreenName);
  EXPECT_CALL(*mock_enable_debugging_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(*mock_enable_debugging_screen_,
         BaseScreenDelegate::ENABLE_DEBUGGING_CANCELED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WizardController::kNetworkScreenName);
}

class WizardControllerOobeResumeTest : public WizardControllerTest {
 protected:
  WizardControllerOobeResumeTest() {}
  // Overriden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    MOCK_WITH_DELEGATE(mock_network_screen_, kNetworkScreenName,
                       MockNetworkScreen, MockNetworkView);
    MOCK(mock_enrollment_screen_,
         kEnrollmentScreenName,
         MockEnrollmentScreen,
         MockEnrollmentScreenActor);
  }

  void OnExit(BaseScreen& screen, BaseScreenDelegate::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(screen, exit_code,
                                                   nullptr /* context */);
  }

  std::string GetFirstScreenName() {
    return WizardController::default_controller()->first_screen_name();
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkView>* mock_network_screen_;
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
  CheckCurrentScreen(WizardController::kNetworkScreenName);
  EXPECT_CALL(*mock_enrollment_screen_->actor(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kEnrollmentScreenName);
  CheckCurrentScreen(WizardController::kEnrollmentScreenName);
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

static_assert(BaseScreenDelegate::EXIT_CODES_COUNT == 23,
              "tests for new control flow are missing");

}  // namespace chromeos

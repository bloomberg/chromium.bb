// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include "ash/accessibility_types.h"
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
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_view.h"
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
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
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
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
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
  content::RunAllTasksUntilIdle();

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

}  // namespace

using ::testing::_;

template <class T, class H>
class MockOutShowHide : public T {
 public:
  template <class P>
  explicit MockOutShowHide(P p) : T(p) {}
  template <class P>
  MockOutShowHide(P p, H* view) : T(p, view), view_(view) {}
  template <class P, class Q>
  MockOutShowHide(P p, Q q, H* view) : T(p, q, view), view_(view) {}

  H* view() const { return view_.get(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void RealShow() { T::Show(); }

  void RealHide() { T::Hide(); }

 private:
  std::unique_ptr<H> view_;
};

#define MOCK(mock_var, screen_name, mocked_class, view_class)  \
  mock_var = new MockOutShowHide<mocked_class, view_class>(    \
      WizardController::default_controller(), new view_class); \
  WizardController::default_controller()                       \
      ->screen_manager()                                       \
      ->screens_[screen_name] = base::WrapUnique(mock_var);    \
  EXPECT_CALL(*mock_var, Show()).Times(0);                     \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

#define MOCK_WITH_DELEGATE(mock_var, screen_name, mocked_class, view_class) \
  mock_var = new MockOutShowHide<mocked_class, view_class>(                 \
      WizardController::default_controller(),                               \
      WizardController::default_controller(), new view_class);              \
  WizardController::default_controller()                                    \
      ->screen_manager()                                                    \
      ->screens_[screen_name] = base::WrapUnique(mock_var);                 \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                  \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

class WizardControllerTest : public WizardInProcessBrowserTest {
 protected:
  WizardControllerTest()
      : WizardInProcessBrowserTest(OobeScreen::SCREEN_TEST_NO_WINDOW) {}
  ~WizardControllerTest() override {}

  void SetUpOnMainThread() override {
    AccessibilityManager::Get()->SetProfileForTest(
        ProfileHelper::GetSigninProfile());
    WizardInProcessBrowserTest::SetUpOnMainThread();
  }

  ErrorScreen* GetErrorScreen() {
    return static_cast<BaseScreenDelegate*>(
               WizardController::default_controller())
        ->GetErrorScreen();
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

  bool JSExecute(const std::string& script) {
    return content::ExecuteScript(GetWebContents(), script);
  }

  bool JSExecuteBooleanExpression(const std::string& expression) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    return result;
  }

  void CheckCurrentScreen(OobeScreen screen) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen),
              WizardController::default_controller()->current_screen());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_NETWORK);

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
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d,
      net::NetworkTrafficAnnotationTag traffic_annotation) override {
    if (base::StartsWith(
            url.spec(),
            SimpleGeolocationProvider::DefaultGeolocationProviderURL().spec(),
            base::CompareCase::SENSITIVE)) {
      return std::unique_ptr<net::URLFetcher>(new net::FakeURLFetcher(
          url, d, std::string(kGeolocationResponseBody), net::HTTP_OK,
          net::URLRequestStatus::SUCCESS));
    }
    if (base::StartsWith(url.spec(),
                         chromeos::DefaultTimezoneProviderURL().spec(),
                         base::CompareCase::SENSITIVE)) {
      return std::unique_ptr<net::URLFetcher>(new net::FakeURLFetcher(
          url, d, std::string(kTimezoneResponseBody), net::HTTP_OK,
          net::URLRequestStatus::SUCCESS));
    }
    return net::TestURLFetcherFactory::CreateURLFetcher(id, url, request_type,
                                                        d, traffic_annotation);
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
    mock_network_screen_ = new MockNetworkScreen(
        WizardController::default_controller(),
        WizardController::default_controller(), GetOobeUI()->GetNetworkView());
    WizardController::default_controller()
        ->screen_manager()
        ->screens_[OobeScreen::SCREEN_OOBE_NETWORK]
        .reset(mock_network_screen_);
    EXPECT_CALL(*mock_network_screen_, Show()).Times(0);
    EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);

    MOCK(mock_update_screen_, OobeScreen::SCREEN_OOBE_UPDATE, MockUpdateScreen,
         MockUpdateView);
    MOCK_WITH_DELEGATE(mock_eula_screen_, OobeScreen::SCREEN_OOBE_EULA,
                       MockEulaScreen, MockEulaView);
    MOCK(mock_enrollment_screen_, OobeScreen::SCREEN_OOBE_ENROLLMENT,
         MockEnrollmentScreen, MockEnrollmentScreenView);
    MOCK(mock_auto_enrollment_check_screen_,
         OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK,
         MockAutoEnrollmentCheckScreen, MockAutoEnrollmentCheckScreenView);
    MOCK(mock_wrong_hwid_screen_, OobeScreen::SCREEN_WRONG_HWID,
         MockWrongHWIDScreen, MockWrongHWIDScreenView);
    MOCK(mock_enable_debugging_screen_,
         OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING, MockEnableDebuggingScreen,
         MockEnableDebuggingScreenView);
    device_disabled_screen_view_.reset(new MockDeviceDisabledScreenView);
    wizard_controller->screen_manager()
        ->screens_[OobeScreen::SCREEN_DEVICE_DISABLED] =
        base::MakeUnique<DeviceDisabledScreen>(
            wizard_controller, device_disabled_screen_view_.get());
    EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(0);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, wizard_controller->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  }

  void TearDownOnMainThread() override {
    mock_network_screen_ = nullptr;
    device_disabled_screen_view_.reset();
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
        default_network->guid(), online_state);
  }

  void OnExit(BaseScreen& screen, ScreenExitCode exit_code) {
    WizardController::default_controller()->OnExit(screen, exit_code,
                                                   nullptr /* context */);
  }

  chromeos::SimpleGeolocationProvider* GetGeolocationProvider() {
    return WizardController::default_controller()->geolocation_provider_.get();
  }

  void WaitUntilTimezoneResolved() {
    std::unique_ptr<TimeZoneTestRunner> runner(new TimeZoneTestRunner);
    if (!WizardController::default_controller()
             ->SetOnTimeZoneResolvedForTesting(
                 base::Bind(&TimeZoneTestRunner::OnResolved,
                            base::Unretained(runner.get()))))
      return;

    runner->Run();
  }

  void ResetAutoEnrollmentCheckScreen() {
    WizardController::default_controller()->screen_manager()->screens_.erase(
        OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  }

  void TestControlFlowMain() {
    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);

    WaitUntilJSIsReady();

    // Check visibility of the header bar.
    ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

    EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
    OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);

    // Header bar should still be visible.
    ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

    EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
    EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
    // Enable TimeZone resolve
    InitTimezoneResolver();
    OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);
    EXPECT_TRUE(GetGeolocationProvider());

    // Let update screen smooth time process (time = 0ms).
    content::RunAllPendingInMessageLoop();

    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
    EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
    OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

    CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
    OnExit(*mock_auto_enrollment_check_screen_,
           ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

    EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
    EXPECT_EQ("ethernet,wifi,cellular", NetworkHandler::Get()
                                            ->network_state_handler()
                                            ->GetCheckPortalListForTest());

    WaitUntilTimezoneResolved();
    EXPECT_EQ(
        "America/Anchorage",
        base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                              ->GetCurrentTimezoneID()));
  }

  MockNetworkScreen* mock_network_screen_;  // Unowned ptr.
  MockOutShowHide<MockUpdateScreen, MockUpdateView>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaView>* mock_eula_screen_;
  MockOutShowHide<MockEnrollmentScreen, MockEnrollmentScreenView>*
      mock_enrollment_screen_;
  MockOutShowHide<MockAutoEnrollmentCheckScreen,
                  MockAutoEnrollmentCheckScreenView>*
      mock_auto_enrollment_check_screen_;
  MockOutShowHide<MockWrongHWIDScreen, MockWrongHWIDScreenView>*
      mock_wrong_hwid_screen_;
  MockOutShowHide<MockEnableDebuggingScreen, MockEnableDebuggingScreenView>*
      mock_enable_debugging_screen_;
  std::unique_ptr<MockDeviceDisabledScreenView> device_disabled_screen_view_;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  // Use a test factory as a fallback so we don't have to deal with other
  // requests.
  std::unique_ptr<WizardControllerTestURLFetcherFactory>
      fallback_fetcher_factory_;
  std::unique_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  TestControlFlowMain();
}

// This test verifies that if WizardController fails to apply a non-critical
// update before the OOBE is marked complete, it allows the user to proceed to
// log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateNonCriticalUpdate) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_ERROR_UPDATING);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This test verifies that if WizardController fails to apply a critical update
// before the OOBE is marked complete, it goes back the network selection
// screen and thus prevents the user from proceeding to log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateCriticalUpdate) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(*mock_update_screen_,
         ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  EXPECT_EQ("ethernet,wifi,cellular", NetworkHandler::Get()
                                          ->network_state_handler()
                                          ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_ENROLLMENT);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(*mock_enrollment_screen_,
         ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->ShowWrongHWIDScreen();

  CheckCurrentScreen(OobeScreen::SCREEN_WRONG_HWID);

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(*mock_wrong_hwid_screen_, ScreenExitCode::WRONG_HWID_WARNING_SKIPPED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This parameterized test class extends WizardControllerFlowTest to verify how
// WizardController behaves if it fails to apply an update after the OOBE is
// marked complete.
class WizardControllerErrorUpdateAfterCompletedOobeTest
    : public WizardControllerFlowTest,
      public testing::WithParamInterface<ScreenExitCode> {
 protected:
  WizardControllerErrorUpdateAfterCompletedOobeTest() = default;

  void SetUpOnMainThread() override {
    StartupUtils::MarkOobeCompleted();  // Pretend OOBE was complete.
    WizardControllerFlowTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerErrorUpdateAfterCompletedOobeTest);
};

// This test verifies that if WizardController fails to apply an update, either
// critical or non-critical, after the OOBE is marked complete, it allows the
// user to proceed to log in.
IN_PROC_BROWSER_TEST_P(WizardControllerErrorUpdateAfterCompletedOobeTest,
                       ControlFlowErrorUpdate) {
  const ScreenExitCode update_screen_exit_code = GetParam();
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, update_screen_exit_code);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_NE(nullptr, ExistingUserController::current_controller());
}

INSTANTIATE_TEST_CASE_P(
    WizardControllerErrorUpdateAfterCompletedOobe,
    WizardControllerErrorUpdateAfterCompletedOobeTest,
    testing::Values(ScreenExitCode::UPDATE_ERROR_UPDATING,
                    ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE));

class WizardControllerDeviceStateTest : public WizardControllerFlowTest {
 protected:
  WizardControllerDeviceStateTest()
      : install_attributes_(ScopedStubInstallAttributes::CreateUnset()) {
    fake_statistics_provider_.SetMachineStatistic(system::kSerialNumberKey,
                                                  "test");
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  "2000-01");
  }

  static void WaitForAutoEnrollmentState(policy::AutoEnrollmentState state) {
    base::RunLoop loop;
    std::unique_ptr<
        AutoEnrollmentController::ProgressCallbackList::Subscription>
        progress_subscription(
            WizardController::default_controller()
                ->GetAutoEnrollmentController()
                ->RegisterProgressCallback(base::Bind(
                    &QuitLoopOnAutoEnrollmentProgress, state, &loop)));
    loop.Run();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        chromeos::AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "1");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "2");
  }

  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerDeviceStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowForcedReEnrollment) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(*mock_enrollment_screen_,
         ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowNoForcedReEnrollmentOnFirstBoot) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            WizardController::default_controller()
                ->GetAutoEnrollmentController()
                ->state());

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            WizardController::default_controller()
                ->GetAutoEnrollmentController()
                ->state());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowDeviceDisabled) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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
  EXPECT_CALL(*device_disabled_screen_view_, UpdateMessage(kDisabledMessage))
      .Times(1);
  EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure the device disabled screen is shown.
  CheckCurrentScreen(OobeScreen::SCREEN_DEVICE_DISABLED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 protected:
  WizardControllerBrokenLocalStateTest() : fake_session_manager_client_(NULL) {}

  ~WizardControllerBrokenLocalStateTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();

    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  void SetUpOnMainThread() override {
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<PrefStoreStub>());
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
  std::unique_ptr<PrefService> local_state_;
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
                      base::FilePath()) {}
  ~WizardControllerProxyAuthOnSigninTest() override {}

  // Overridden from WizardControllerTest:
  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    WizardControllerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
    WizardController::default_controller()->AdvanceToScreen(
        OobeScreen::SCREEN_OOBE_NETWORK);
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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  auth_needed_waiter.Wait();
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerKioskFlowTest() {}

  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
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
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(*mock_enrollment_screen_,
         ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBack) {
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(*mock_network_screen_, ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(*mock_eula_screen_, ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_update_screen_, ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(1);
  OnExit(*mock_auto_enrollment_check_screen_,
         ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(*mock_enrollment_screen_, ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enable_debugging_screen_, Show()).Times(1);

  ASSERT_TRUE(JSExecute("$('connect-debugging-features-link').click()"));

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING);
  EXPECT_CALL(*mock_enable_debugging_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(*mock_enable_debugging_screen_,
         ScreenExitCode::ENABLE_DEBUGGING_CANCELED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
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
    MOCK_WITH_DELEGATE(mock_network_screen_, OobeScreen::SCREEN_OOBE_NETWORK,
                       MockNetworkScreen, MockNetworkView);
    MOCK(mock_enrollment_screen_, OobeScreen::SCREEN_OOBE_ENROLLMENT,
         MockEnrollmentScreen, MockEnrollmentScreenView);
  }

  void OnExit(BaseScreen& screen, ScreenExitCode exit_code) {
    WizardController::default_controller()->OnExit(screen, exit_code,
                                                   nullptr /* context */);
  }

  OobeScreen GetFirstScreen() {
    return WizardController::default_controller()->first_screen();
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkView>* mock_network_screen_;
  MockOutShowHide<MockEnrollmentScreen, MockEnrollmentScreenView>*
      mock_enrollment_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeResumeTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       PRE_ControlFlowResumeInterruptedOobe) {
  // Switch to the initial screen.
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_NETWORK);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_ENROLLMENT);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
}

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       ControlFlowResumeInterruptedOobe) {
  EXPECT_EQ(OobeScreen::SCREEN_OOBE_ENROLLMENT, GetFirstScreen());
}

class WizardControllerCellularFirstTest : public WizardControllerFlowTest {
 protected:
  WizardControllerCellularFirstTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kCellularFirst);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerCellularFirstTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerCellularFirstTest, CellularFirstFlow) {
  TestControlFlowMain();
}

// TODO(dzhioev): Add test emulating device with wrong HWID.

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

// TODO(merkulova): Add tests for bluetooth HID detection screen variations when
// UI and logic is ready. http://crbug.com/127016

// TODO(dzhioev): Add tests for controller/host pairing flow.
// http://crbug.com/375191

// TODO(khmel): Add tests for ARC OptIn flow.
// http://crbug.com/651144

// TODO(fukino): Add tests for encryption migration UI.
// http://crbug.com/706017

// TODO(updowndota): Add tests for Voice Interaction OptIn flow.
static_assert(static_cast<int>(ScreenExitCode::EXIT_CODES_COUNT) == 32,
              "tests for new control flow are missing");

}  // namespace chromeos

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/test/dialog_window_waiter.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/scoped_help_app_for_test.h"
#include "chrome/browser/chromeos/login/test/webview_content_extractor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using ::testing::ElementsAre;

namespace chromeos {
namespace {

constexpr char kFakeOnlineEulaPath[] = "/intl/en-US/chrome/eula_text.html";
constexpr char kFakeOnlineEula[] = "No obligations at all";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// See IDS_TERMS_HTML for the complete text.
constexpr char kOfflineEULAWarning[] =
    "Google Chrome and Chrome OS Additional Terms of Service";
#else
// Placeholder text in terms_chromium.html.
constexpr char kOfflineEULAWarning[] =
    "In official builds this space will show the terms of service.";
#endif

// Helper class to wait until the WebCotnents finishes loading.
class WebContentsLoadFinishedWaiter : public content::WebContentsObserver {
 public:
  explicit WebContentsLoadFinishedWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~WebContentsLoadFinishedWaiter() override = default;

  void Wait() {
    if (!web_contents()->IsLoading() &&
        web_contents()->GetLastCommittedURL() != GURL::EmptyGURL()) {
      return;
    }

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsLoadFinishedWaiter);
};

// Helper invoked by GuestViewManager::ForEachGuest to collect WebContents of
// Webview named as |web_view_name,|.
bool AddNamedWebContentsToSet(std::set<content::WebContents*>* frame_set,
                              const std::string& web_view_name,
                              content::WebContents* web_contents) {
  auto* web_view = extensions::WebViewGuest::FromWebContents(web_contents);
  if (web_view && web_view->name() == web_view_name)
    frame_set->insert(web_contents);
  return false;
}

class EulaTest : public OobeBaseTest {
 public:
  EulaTest() = default;
  ~EulaTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Retrieve the URL from the embedded test server and override EULA URL.
    std::string fake_eula_url =
        embedded_test_server()->base_url().Resolve(kFakeOnlineEulaPath).spec();
    command_line->AppendSwitchASCII(switches::kOobeEulaUrlForTests,
                                    fake_eula_url);
  }

  // OobeBaseTest:
  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&EulaTest::HandleRequest, base::Unretained(this)));
  }

  void ShowEulaScreen() {
    LoginDisplayHost::default_host()->StartWizard(EulaView::kScreenId);
    OobeScreenWaiter(EulaView::kScreenId).Wait();
  }

 protected:
  // Used for customizing the response handler of the embedded server.
  void set_force_http_unavailable(bool force_unavailable) {
    force_http_unavailable_ = force_unavailable;
  }

  content::WebContents* FindEulaContents() {
    // Tag the Eula webview in use with a unique name.
    constexpr char kUniqueEulaWebviewName[] = "unique-eula-webview-name";
    test::OobeJS().Evaluate(base::StringPrintf(
        "(function(){"
        "  var eulaWebView = $('oobe-eula-md').$.crosEulaFrame;"
        "  eulaWebView.name = '%s';"
        "})();",
        kUniqueEulaWebviewName));

    // Find the WebContents tagged with the unique name.
    std::set<content::WebContents*> frame_set;
    auto* const owner_contents = GetLoginUI()->GetWebContents();
    auto* const manager = guest_view::GuestViewManager::FromBrowserContext(
        owner_contents->GetBrowserContext());
    manager->ForEachGuest(
        owner_contents,
        base::BindRepeating(&AddNamedWebContentsToSet, &frame_set,
                            kUniqueEulaWebviewName));
    EXPECT_EQ(1u, frame_set.size());
    return *frame_set.begin();
  }

  // Wait for the fallback offline page (loaded as data url) to be loaded.
  void WaitForLocalWebviewLoad() {
    content::WebContents* eula_contents = FindEulaContents();
    ASSERT_TRUE(eula_contents);

    while (!eula_contents->GetLastCommittedURL().SchemeIs("data")) {
      // Pump messages to avoid busy loop so that renderer could do some work.
      base::RunLoop().RunUntilIdle();
      WebContentsLoadFinishedWaiter(eula_contents).Wait();
    }
  }

  // Returns an Oobe JSChecker that sends 'click' events instead of 'tap'
  // events when interacting with UI elements.
  test::JSChecker NonPolymerOobeJS() {
    test::JSChecker js = test::OobeJS();
    js.set_polymer_ui(false);
    return js;
  }

  base::OnceClosure SetCollectStatsConsentClosure(bool consented) {
    return base::BindOnce(
        base::IgnoreResult(&GoogleUpdateSettings::SetCollectStatsConsent),
        consented);
  }

  // Calls |GoogleUpdateSettings::SetCollectStatsConsent| asynchronously on its
  // task runner. Blocks until task is executed.
  void SetGoogleCollectStatsConsent(bool consented) {
    base::RunLoop runloop;
    GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTaskAndReply(
        FROM_HERE, SetCollectStatsConsentClosure(consented),
        runloop.QuitClosure());
    runloop.Run();
  }

  // Calls |GoogleUpdateSettings::GetCollectStatsConsent| asynchronously on its
  // task runner. Blocks until task is executed and returns the result.
  bool GetGoogleCollectStatsConsent() {
    bool consented = false;

    // Callback runs after GetCollectStatsConsent is executed. Sets the local
    // variable |consented| to the result of GetCollectStatsConsent.
    auto on_get_collect_stats_consent_callback =
        [](base::OnceClosure quit_closure, bool* consented_out,
           bool consented_result) {
          *consented_out = consented_result;
          std::move(quit_closure).Run();
        };

    base::RunLoop runloop;
    base::PostTaskAndReplyWithResult(
        GoogleUpdateSettings::CollectStatsConsentTaskRunner(), FROM_HERE,
        base::BindOnce(&GoogleUpdateSettings::GetCollectStatsConsent),
        base::BindOnce(on_get_collect_stats_consent_callback,
                       runloop.QuitClosure(), &consented));
    runloop.Run();

    return consented;
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    const std::string request_path = request_url.path();
    if (!base::EndsWith(request_path, "/eula_text.html",
                        base::CompareCase::SENSITIVE)) {
      return std::unique_ptr<HttpResponse>();
    }

    std::unique_ptr<BasicHttpResponse> http_response =
        std::make_unique<BasicHttpResponse>();

    if (force_http_unavailable_) {
      http_response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
    } else {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(kFakeOnlineEula);
    }

    return std::move(http_response);
  }

  // The default behaviour for the embedded server is to service the
  // online version properly. Offline tests may change this during construction
  // of the class.
  bool force_http_unavailable_ = false;

  DISALLOW_COPY_AND_ASSIGN(EulaTest);
};

// When testing the offline fallback mechanism, the requests reaching the
// embedded server have to be handled differently.
class EulaOfflineTest : public EulaTest {
 public:
  EulaOfflineTest() { set_force_http_unavailable(true); }

  ~EulaOfflineTest() override = default;
};

// Tests that offline version is shown when the online version is not
// accessible.
IN_PROC_BROWSER_TEST_F(EulaOfflineTest, LoadOffline) {
  ShowEulaScreen();

  WaitForLocalWebviewLoad();
  EXPECT_TRUE(test::GetWebViewContents({"oobe-eula-md", "crosEulaFrame"})
                  .find(kOfflineEULAWarning) != std::string::npos);
}

#if defined(OS_CHROMEOS) && \
    (defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER))
// TODO(http://crbug.com/1041188): flaky on ChromeOS MSAN and LSAN.
#define MAYBE_LoadOnline DISABLED_LoadOnline
#else
#define MAYBE_LoadOnline LoadOnline
#endif
// Tests that online version is shown when it is accessible.
IN_PROC_BROWSER_TEST_F(EulaTest, MAYBE_LoadOnline) {
  ShowEulaScreen();

  // Wait until the webview has finished loading.
  content::WebContents* eula_contents = FindEulaContents();
  ASSERT_TRUE(eula_contents);
  WebContentsLoadFinishedWaiter(eula_contents).Wait();

  // Wait until the Accept button on the EULA frame becomes enabled.
  chromeos::test::OobeJS()
      .CreateEnabledWaiter(true, {"oobe-eula-md", "acceptButton"})
      ->Wait();

  const std::string webview_contents =
      test::GetWebViewContents({"oobe-eula-md", "crosEulaFrame"});
  EXPECT_TRUE(webview_contents.find(kFakeOnlineEula) != std::string::npos);
}

// Tests that clicking on "System security settings" button opens a dialog
// showing the TPM password.
IN_PROC_BROWSER_TEST_F(EulaTest, DisplaysTpmPassword) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "installationSettings"});
  test::OobeJS().ExpectVisiblePath(
      {"oobe-eula-md", "installationSettingsDialog"});

  test::OobeJS()
      .CreateWaiter(
          "$('oobe-eula-md').$$('#eula-password').textContent.trim() !== ''")
      ->Wait();
  test::OobeJS().ExpectEQ(
      "$('oobe-eula-md').$$('#eula-password').textContent.trim()",
      std::string(FakeCryptohomeClient::kStubTpmPassword));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(EulaScreen::UserAction::kShowSecuritySettings), 1)));
}

// Verifies statistic collection accepted flow.
// Advaces to the next screen and verifies stats collection is enabled.
IN_PROC_BROWSER_TEST_F(EulaTest, EnableUsageStats) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Verify that toggle is enabled by default.
  test::OobeJS().ExpectTrue("$('oobe-eula-md').$$('#usageStats').checked");

  ASSERT_TRUE(StatsReportingController::IsInitialized());

  // Explicitly set as false to make sure test modifies these values.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), false);
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, false);
  SetGoogleCollectStatsConsent(false);

  // Start Listening for StatsReportingController updates.
  base::RunLoop runloop;
  auto subscription =
      StatsReportingController::Get()->AddObserver(runloop.QuitClosure());

  // Enable and disable usageStats that to see that metrics are recorded.
  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "usageStats"});
  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "usageStats"});
  // Advance to the next screen for changes to take effect.
  test::OobeJS().TapOnPath({"oobe-eula-md", "acceptButton"});

  // Wait for StartReporting update.
  runloop.Run();

  // Verify stats collection is enabled.
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_TRUE(GetGoogleCollectStatsConsent());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kAcceptButtonClicked),
              1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kUnselectStatsUsage), 1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kSelectStatsUsage), 1)));
}

// Verify statistic collection denied flow. Clicks on usage stats toggle,
// advaces to the next screen and verifies stats collection is disabled.
IN_PROC_BROWSER_TEST_F(EulaTest, DisableUsageStats) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Verify that toggle is enabled by default.
  test::OobeJS().ExpectTrue("$('oobe-eula-md').$$('#usageStats').checked");

  ASSERT_TRUE(StatsReportingController::IsInitialized());

  // Explicitly set as true to make sure test modifies these values.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), true);
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, true);
  SetGoogleCollectStatsConsent(true);

  // Start Listening for StatsReportingController updates.
  base::RunLoop runloop;
  auto subscription =
      StatsReportingController::Get()->AddObserver(runloop.QuitClosure());

  // Click on the toggle to disable stats collection and advance to the next
  // screen for changes to take effect.
  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "usageStats"});
  test::OobeJS().TapOnPath({"oobe-eula-md", "acceptButton"});

  // Wait for StartReportingController update.
  runloop.Run();

  // Verify stats collection is disabled.
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_FALSE(GetGoogleCollectStatsConsent());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kAcceptButtonClicked),
              1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kUnselectStatsUsage),
              1)));
}

// Tests that clicking on "Learn more" button opens a help dialog.
IN_PROC_BROWSER_TEST_F(EulaTest, LearnMore) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Load HelperApp extension.
  ScopedHelpAppForTest scoped_help_app;

  // Start listening for help dialog creation.
  DialogWindowWaiter waiter(
      l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE));

  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "learn-more"});

  // Wait until help dialog is displayed.
  waiter.Wait();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(EulaScreen::UserAction::kShowStatsUsageLearnMore),
          1)));
}

// Tests that "Additional ToS" dialog could be opened and closed.
IN_PROC_BROWSER_TEST_F(EulaTest, AdditionalToS) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "additionalTerms"});

  test::OobeJS()
      .CreateWaiter(
          test::GetOobeElementPath({"oobe-eula-md", "additional-tos"}) +
          ".open")
      ->Wait();

  NonPolymerOobeJS().TapOnPath({"oobe-eula-md", "close-additional-tos"});

  test::OobeJS()
      .CreateWaiter(
          test::GetOobeElementPath({"oobe-eula-md", "additional-tos"}) +
          ".open === false")
      ->Wait();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(EulaScreen::UserAction::kShowAdditionalTos), 1)));
}

}  // namespace

}  // namespace chromeos

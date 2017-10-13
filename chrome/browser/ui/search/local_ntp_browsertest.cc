// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/json/string_escape.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"

using search_provider_logos::EncodedLogo;
using search_provider_logos::EncodedLogoCallback;
using search_provider_logos::LogoCallbacks;
using search_provider_logos::LogoCallbackReason;
using search_provider_logos::LogoObserver;
using search_provider_logos::LogoService;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsEmpty;

namespace {

const char kCachedB64[] = "\161\247\041\171\337\276";  // b64decode("cached++")
const char kFreshB64[] = "\176\267\254\207\357\276";   // b64decode("fresh+++")

scoped_refptr<base::RefCountedString> MakeRefPtr(std::string content) {
  return base::RefCountedString::TakeString(&content);
}

content::WebContents* OpenNewTab(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  return browser->tab_strip_model()->GetActiveWebContents();
}

// Switches the browser language to French, and returns true iff successful.
bool SwitchToFrench() {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  // Make sure the default language is not French.
  std::string default_locale = g_browser_process->GetApplicationLocale();
  EXPECT_NE("fr", default_locale);

  // Switch browser language to French.
  g_browser_process->SetApplicationLocale("fr");
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, "fr");

  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("fr");

  return loaded_locale == "fr";
}

}  // namespace

class LocalNTPTest : public InProcessBrowserTest {
 public:
  LocalNTPTest() {}

  // Navigates the active tab to chrome://newtab and waits until the NTP is
  // fully loaded. Note that simply waiting for a navigation is not enough,
  // since the MV iframe receives the tiles asynchronously.
  void NavigateToNTPAndWaitUntilLoaded() {
    content::WebContents* active_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Attach a message queue *before* navigating to the NTP, to make sure we
    // don't miss the 'loaded' message.
    content::DOMMessageQueue msg_queue(active_tab);

    // Navigate to the NTP.
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
    ASSERT_TRUE(search::IsInstantNTP(active_tab));
    ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
              active_tab->GetController().GetVisibleEntry()->GetURL());

    // When the iframe has loaded all the tiles, it sends a 'loaded' postMessage
    // to the page. Wait for that message to arrive.
    ASSERT_TRUE(content::ExecuteScript(active_tab, R"js(
      window.addEventListener('message', function(event) {
        if (event.data.cmd == 'loaded') {
          domAutomationController.send('NavigateToNTPAndWaitUntilLoaded');
        }
      });
    )js"));
    std::string message;
    // First get rid of a message produced by the ExecuteScript call above.
    ASSERT_TRUE(msg_queue.PopMessage(&message));
    // Now wait for the "NavigateToNTPAndWaitUntilLoaded" message.
    ASSERT_TRUE(msg_queue.WaitForMessage(&message));
    ASSERT_EQ("\"NavigateToNTPAndWaitUntilLoaded\"", message);
    // There shouldn't be any other messages.
    ASSERT_FALSE(msg_queue.PopMessage(&message));
  }

  void SetUserSelectedDefaultSearchProvider(const std::string& base_url,
                                            const std::string& ntp_url) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(base_url));
    data.SetKeyword(base::UTF8ToUTF16(base_url));
    data.SetURL(base_url + "url?bar={searchTerms}");
    data.new_tab_url = ntp_url;

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

 private:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kUseGoogleLocalNtp);
    InProcessBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIOnlyAvailableOnNTP) {
  // Set up a test server, so we have some arbitrary non-NTP URL to navigate to.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(test_server.Start());
  const GURL other_url = test_server.GetURL("/simple.html");

  // Open an NTP.
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the embeddedSearch API is available.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate somewhere else in the same tab.
  content::TestNavigationObserver elsewhere_observer(active_tab);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  elsewhere_observer.Wait();
  ASSERT_TRUE(elsewhere_observer.last_navigation_succeeded());
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Now the embeddedSearch API should have gone away.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate back to the NTP.
  content::TestNavigationObserver back_observer(active_tab);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_observer.Wait();
  // The API should be back.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate forward to the non-NTP page.
  content::TestNavigationObserver fwd_observer(active_tab);
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  fwd_observer.Wait();
  // The API should be gone.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_FALSE(result);

  // Navigate to a new NTP instance.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Now the API should be available again.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NTPRespectsBrowserLanguageSetting) {
  // If the platform cannot load the French locale (GetApplicationLocale() is
  // platform specific, and has been observed to fail on a small number of
  // platforms), abort the test.
  if (!SwitchToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a new tab.
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Verify that the NTP is in French.
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, GoogleNTPLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  NavigateToNTPAndWaitUntilLoaded();

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_TRUE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Google", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 2 tiles, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 2, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 2);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 2);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 2);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, NonGoogleNTPLoadsWithoutError) {
  SetUserSelectedDefaultSearchProvider("https://www.example.com",
                                       /*ntp_url=*/"");

  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  base::HistogramTester histograms;

  // Navigate to the NTP.
  NavigateToNTPAndWaitUntilLoaded();

  bool is_google = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.configData && !!window.configData.isGooglePage",
      &is_google));
  EXPECT_FALSE(is_google);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();

  // Make sure load time metrics were recorded.
  histograms.ExpectTotalCount("NewTabPage.LoadTime", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.LocalNTP.Other", 1);
  histograms.ExpectTotalCount("NewTabPage.LoadTime.MostVisited", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.LocalNTP", 1);
  histograms.ExpectTotalCount("NewTabPage.TilesReceivedTime.MostVisited", 1);

  // Make sure impression metrics were recorded. There should be 2 tiles, the
  // default prepopulated TopSites (see history::PrepopulatedPage).
  histograms.ExpectTotalCount("NewTabPage.NumberOfTiles", 1);
  histograms.ExpectBucketCount("NewTabPage.NumberOfTiles", 2, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression", 2);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 0, 1);
  histograms.ExpectBucketCount("NewTabPage.SuggestionsImpression", 1, 1);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.client", 2);
  histograms.ExpectTotalCount("NewTabPage.SuggestionsImpression.Thumbnail", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle", 2);
  histograms.ExpectTotalCount("NewTabPage.TileTitle.client", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType", 2);
  histograms.ExpectTotalCount("NewTabPage.TileType.client", 2);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, FrenchGoogleNTPLoadsWithoutError) {
  if (!SwitchToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());
  // Make sure it's actually in French.
  ASSERT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

class LocalNTPRTLTest : public LocalNTPTest {
 public:
  LocalNTPRTLTest() {}

 private:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    cmdline->AppendSwitchASCII(switches::kForceUIDirection,
                               switches::kForceDirectionRTL);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPRTLTest, RightToLeft) {
  // Open an NTP.
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the "dir" attribute on the main "html" element says "rtl".
  std::string dir;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.documentElement.dir", &dir));
  EXPECT_EQ("rtl", dir);
}

// A test class that sets up a local HTML file as the NTP URL.
class CustomNTPUrlTest : public LocalNTPTest {
 public:
  explicit CustomNTPUrlTest(const std::string& ntp_file_path)
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        ntp_file_path_(ntp_file_path) {
    https_test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/local_ntp");
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_test_server_.Start());
    GURL ntp_url = https_test_server_.GetURL(ntp_file_path_);
    SetUserSelectedDefaultSearchProvider(https_test_server_.base_url().spec(),
                                         ntp_url.spec());
  }

  net::EmbeddedTestServer https_test_server_;
  const std::string ntp_file_path_;
};

// A test class that sets up local_ntp_browsertest.html as the NTP URL. It's
// mostly a copy of the real local_ntp.html, but it adds some testing JS.
class LocalNTPJavascriptTest : public CustomNTPUrlTest {
 public:
  LocalNTPJavascriptTest() : CustomNTPUrlTest("/local_ntp_browsertest.html") {}
};

// This runs a bunch of pure JS-side tests, i.e. those that don't require any
// interaction from the native side.
IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, SimpleJavascriptTests) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('localNtp')", &success));
  EXPECT_TRUE(success);
}

// A test class that sets up voice_browsertest.html as the NTP URL. It's
// mostly a copy of the real local_ntp.html, but it adds some testing JS.
class LocalNTPVoiceJavascriptTest : public CustomNTPUrlTest {
 public:
  LocalNTPVoiceJavascriptTest() : CustomNTPUrlTest("/voice_browsertest.html") {}
};

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, MicrophoneTests) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('microphone')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, TextTests) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('text')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, SpeechTests) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('speech')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, ViewTests) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('view')", &success));
  EXPECT_TRUE(success);
}

namespace {

// Returns the RenderFrameHost corresponding to the most visited iframe in the
// given |tab|. |tab| must correspond to an NTP.
content::RenderFrameHost* GetMostVisitedIframe(content::WebContents* tab) {
  CHECK_EQ(2u, tab->GetAllFrames().size());
  for (content::RenderFrameHost* frame : tab->GetAllFrames()) {
    if (frame != tab->GetMainFrame()) {
      return frame;
    }
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, LoadsIframe) {
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  content::DOMMessageQueue msg_queue;

  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!setupAdvancedTest(true)", &result));
  ASSERT_TRUE(result);

  // Wait for the MV iframe to load.
  std::string message;
  // First get rid of the "true" message from the GetBoolFromJS call above.
  ASSERT_TRUE(msg_queue.PopMessage(&message));
  ASSERT_EQ("true", message);
  // Now wait for the "loaded" message.
  ASSERT_TRUE(msg_queue.WaitForMessage(&message));
  ASSERT_EQ("\"loaded\"", message);

  // Get the iframe and check that the tiles loaded correctly.
  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Get the total number of (non-empty) tiles from the iframe.
  int total_thumbs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb').length", &total_thumbs));
  // Also get how many of the tiles succeeded and failed in loading their
  // thumbnail images.
  int succeeded_imgs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb img').length",
      &succeeded_imgs));
  int failed_imgs = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      iframe, "document.querySelectorAll('.mv-thumb.failed-img').length",
      &failed_imgs));

  // First, sanity check that the numbers line up (none of the css classes was
  // renamed, etc).
  EXPECT_EQ(total_thumbs, succeeded_imgs + failed_imgs);

  // Since we're in a non-signed-in, fresh profile with no history, there should
  // be the default TopSites tiles (see history::PrepopulatedPage).
  // Check that there is at least one tile, and that all of them loaded their
  // images successfully.
  EXPECT_GT(total_thumbs, 0);
  EXPECT_EQ(total_thumbs, succeeded_imgs);
  EXPECT_EQ(0, failed_imgs);
}

// A simple fake implementation of OneGoogleBarFetcher that immediately returns
// a pre-configured OneGoogleBarData, which is null by default.
class FakeOneGoogleBarFetcher : public OneGoogleBarFetcher {
 public:
  void Fetch(OneGoogleCallback callback) override {
    std::move(callback).Run(Status::OK, one_google_bar_data_);
  }

  void set_one_google_bar_data(
      const base::Optional<OneGoogleBarData>& one_google_bar_data) {
    one_google_bar_data_ = one_google_bar_data;
  }

 private:
  base::Optional<OneGoogleBarData> one_google_bar_data_;
};

class LocalNTPOneGoogleBarSmokeTest : public InProcessBrowserTest {
 public:
  LocalNTPOneGoogleBarSmokeTest() {}

  FakeOneGoogleBarFetcher* one_google_bar_fetcher() {
    return static_cast<FakeOneGoogleBarFetcher*>(
        OneGoogleBarServiceFactory::GetForProfile(browser()->profile())
            ->fetcher_for_testing());
  }

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kOneGoogleBarOnLocalNtp}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&LocalNTPOneGoogleBarSmokeTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateOneGoogleBarService(
      content::BrowserContext* context) {
    GaiaCookieManagerService* cookie_service =
        GaiaCookieManagerServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
    return base::MakeUnique<OneGoogleBarService>(
        cookie_service, base::MakeUnique<FakeOneGoogleBarFetcher>());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    OneGoogleBarServiceFactory::GetInstance()->SetTestingFactory(
        context, &LocalNTPOneGoogleBarSmokeTest::CreateOneGoogleBarService);
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPOneGoogleBarSmokeTest,
                       NTPLoadsWithoutErrorOnNetworkFailure) {
  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

IN_PROC_BROWSER_TEST_F(LocalNTPOneGoogleBarSmokeTest,
                       NTPLoadsWithOneGoogleBar) {
  OneGoogleBarData data;
  data.bar_html = "<div id='thebar'></div>";
  data.in_head_script = "window.inHeadRan = true;";
  data.after_bar_script = "window.afterBarRan = true;";
  data.end_of_body_script = "console.log('ogb-done');";
  one_google_bar_fetcher()->set_one_google_bar_data(data);

  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for the "ogb-done" message, which
  // indicates that the OGB has finished loading.
  content::ConsoleObserverDelegate console_observer(active_tab, "ogb-done");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());
  // Make sure the OGB is finished loading.
  console_observer.Wait();

  EXPECT_EQ("ogb-done", console_observer.message());

  bool in_head_ran = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.inHeadRan", &in_head_ran));
  EXPECT_TRUE(in_head_ran);
  bool after_bar_ran = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.afterBarRan", &after_bar_ran));
  EXPECT_TRUE(after_bar_ran);
}

class LocalNTPVoiceSearchSmokeTest : public InProcessBrowserTest {
 public:
  LocalNTPVoiceSearchSmokeTest() {}

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kVoiceSearchOnLocalNtp}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmdline) override {
    // Requesting microphone permission doesn't work unless there's a device
    // available.
    cmdline->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest,
                       GoogleNTPWithVoiceLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  // Make sure the microphone icon in the fakebox is present and visible.
  bool fakebox_microphone_is_visible = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "!!document.getElementById('fakebox-microphone') && "
      "!document.getElementById('fakebox-microphone').hidden",
      &fakebox_microphone_is_visible));
  EXPECT_TRUE(fakebox_microphone_is_visible);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest, MicrophonePermission) {
  // Open a new NTP.
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  PermissionRequestManager* request_manager =
      PermissionRequestManager::FromWebContents(active_tab);
  MockPermissionPromptFactory prompt_factory(request_manager);

  PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(browser()->profile());

  // Make sure microphone permission for the NTP isn't set yet.
  const PermissionResult mic_permission_before =
      permission_manager->GetPermissionStatus(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin(),
          GURL(chrome::kChromeUINewTabURL).GetOrigin());
  ASSERT_EQ(CONTENT_SETTING_ASK, mic_permission_before.content_setting);
  ASSERT_EQ(PermissionStatusSource::UNSPECIFIED, mic_permission_before.source);

  ASSERT_EQ(0, prompt_factory.TotalRequestCount());

  // Auto-approve the permissions bubble as soon as it shows up.
  prompt_factory.set_response_type(PermissionRequestManager::ACCEPT_ALL);

  // Click on the microphone button, which will trigger a permission request.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('fakebox-microphone').click();"));

  // Make sure the request arrived.
  prompt_factory.WaitForPermissionBubble();
  EXPECT_EQ(1, prompt_factory.show_count());
  EXPECT_EQ(1, prompt_factory.request_count());
  EXPECT_EQ(1, prompt_factory.TotalRequestCount());
  EXPECT_TRUE(prompt_factory.RequestTypeSeen(
      PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
  // ...and that it showed the Google base URL, not the NTP URL.
  const GURL google_base_url(
      UIThreadSearchTermsData(browser()->profile()).GoogleBaseURLValue());
  EXPECT_TRUE(prompt_factory.RequestOriginSeen(google_base_url.GetOrigin()));
  EXPECT_FALSE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeUINewTabURL).GetOrigin()));
  EXPECT_FALSE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin()));

  // Now microphone permission for the NTP should be set.
  const PermissionResult mic_permission_after =
      permission_manager->GetPermissionStatus(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin(),
          GURL(chrome::kChromeUINewTabURL).GetOrigin());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, mic_permission_after.content_setting);
}

class MockLogoService : public LogoService {
 public:
  MOCK_METHOD1(GetLogoPtr, void(LogoCallbacks* callbacks));

  void GetLogo(LogoCallbacks callbacks) override { GetLogoPtr(&callbacks); }
  void GetLogo(LogoObserver* observer) override { NOTREACHED(); }
};

ACTION_P2(ReturnCachedLogo, reason, logo) {
  if (arg0->on_cached_encoded_logo_available) {
    std::move(arg0->on_cached_encoded_logo_available).Run(reason, logo);
  }
}

ACTION_P2(ReturnFreshLogo, reason, logo) {
  if (arg0->on_fresh_encoded_logo_available) {
    std::move(arg0->on_fresh_encoded_logo_available).Run(reason, logo);
  }
}

class LocalNTPDoodleTest : public InProcessBrowserTest {
 protected:
  LocalNTPDoodleTest() {}

  MockLogoService* logo_service() {
    return static_cast<MockLogoService*>(
        LogoServiceFactory::GetForProfile(browser()->profile()));
  }

  base::Optional<double> GetComputedOpacity(content::WebContents* tab,
                                            const std::string& id) {
    std::string css_value;
    double double_value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf(
                "getComputedStyle(document.getElementById(%s)).opacity",
                base::GetQuotedJSONString(id).c_str()),
            &css_value) &&
        base::StringToDouble(css_value, &double_value)) {
      return double_value;
    }
    return base::nullopt;
  }

  // Gets $(id)[property]. Coerces to string.
  base::Optional<std::string> GetElementProperty(content::WebContents* tab,
                                                 const std::string& id,
                                                 const std::string& property) {
    std::string value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf("document.getElementById(%s)[%s] + ''",
                               base::GetQuotedJSONString(id).c_str(),
                               base::GetQuotedJSONString(property).c_str()),
            &value)) {
      return value;
    }
    return base::nullopt;
  }

  void WaitForFadeIn(content::WebContents* tab, const std::string& id) {
    content::ConsoleObserverDelegate console_observer(tab, "WaitForFadeIn");
    tab->SetDelegate(&console_observer);

    bool result = false;
    if (!instant_test_utils::GetBoolFromJS(
            tab,
            base::StringPrintf(
                R"js(
                  (function(id, message) {
                    var element = document.getElementById(id);
                    var fn = function() {
                      if ((element.style.opacity == 1.0) &&
                          (window.getComputedStyle(element).opacity == 1.0)) {
                        console.log(message);
                      } else {
                        element.addEventListener('transitionend', fn);
                      }
                    };
                    fn();
                    return true;
                  })(%s, 'WaitForFadeIn')
                )js",
                base::GetQuotedJSONString(id).c_str()),
            &result) &&
        result) {
      ADD_FAILURE() << "failed to wait for fade-in";
      return;
    }

    console_observer.Wait();
  }

  // See enum LogoImpressionType in ntp_user_data_logger.cc.
  static const int kLogoImpressionStatic = 0;
  static const int kLogoImpressionCta = 1;

  // See enum LogoClickType in ntp_user_data_logger.cc.
  static const int kLogoClickCta = 1;

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kDoodlesOnLocalNtp}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(
                    &LocalNTPDoodleTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateLogoService(
      content::BrowserContext* context) {
    return base::MakeUnique<MockLogoService>();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    LogoServiceFactory::GetInstance()->SetTestingFactory(
        context, &LocalNTPDoodleTest::CreateLogoService);
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedOnLogoFetchCancelled) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(
          DoAll(ReturnCachedLogo(LogoCallbackReason::CANCELED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::CANCELED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedWhenNoCachedOrFreshDoodle) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldShowDoodleWhenCached) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDoodleToDefaultWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, base::nullopt)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-default");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDefaultToDoodleWhenFetched) {
  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  fresh_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDoodleToDoodleWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,fresh+++"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // LogoShown is recorded for both cached and fresh Doodle, but LogoShownTime2
  // is only recorded once per NTP.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 2);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               2);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldUpdateMetadataWhenChanged) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = cached_logo.encoded_image;
  fresh_logo.metadata.mime_type = cached_logo.metadata.mime_type;
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // Metadata update does not count as a new impression.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldAnimateLogoWhenClicked) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.animated_url =
      GURL("https://www.chromium.org/animated.gif");
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,cached++"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("alt text"));

  // Click image, swapping out for animated URL.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq(cached_logo.metadata.animated_url.spec()));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
  histograms.ExpectTotalCount("NewTabPage.LogoClick", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoClick", kLogoClickCta, 1);
}

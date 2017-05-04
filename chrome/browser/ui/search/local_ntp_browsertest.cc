// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

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

// A test class that sets up local_ntp_browsertest.html (which is mostly a copy
// of the real local_ntp.html) as the NTP URL.
class LocalNTPTest : public InProcessBrowserTest, public InstantTestBase {
 public:
  LocalNTPTest() {}

  GURL other_url() { return https_test_server().GetURL("/simple.html"); }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url =
        https_test_server().GetURL("/instant_extended.html?strk=1&");
    GURL ntp_url =
        https_test_server().GetURL("/local_ntp_browsertest.html?strk=1&");
    InstantTestBase::Init(instant_url, ntp_url, false);
  }
};

// This runs a bunch of pure JS-side tests, i.e. those that don't require any
// interaction from the native side.
IN_PROC_BROWSER_TEST_F(LocalNTPTest, SimpleJavascriptTests) {
  if (content::AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "LocalNTPTest.SimpleJavascriptTests doesn't work in "
                  "--site-per-process mode yet, see crbug.com/695221.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  content::WebContents* active_tab = OpenNewTab(browser(), ntp_url());
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests()", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPTest, EmbeddedSearchAPIOnlyAvailableOnNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Open an NTP.
  content::WebContents* active_tab = OpenNewTab(browser(), ntp_url());
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Check that the embeddedSearch API is available.
  bool result = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);

  // Navigate somewhere else in the same tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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
      browser(), ntp_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  // Now the API should be available again.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.chrome.embeddedSearch", &result));
  EXPECT_TRUE(result);
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

IN_PROC_BROWSER_TEST_F(LocalNTPTest, LoadsIframe) {
  if (content::AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "LocalNTPTest.LoadsIframe doesn't work in "
                  "--site-per-process mode yet, see crbug.com/695221.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  content::WebContents* active_tab = OpenNewTab(browser(), ntp_url());
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

IN_PROC_BROWSER_TEST_F(LocalNTPTest,
                       NTPRespectsBrowserLanguageSetting) {
  // If the platform cannot load the French locale (GetApplicationLocale() is
  // platform specific, and has been observed to fail on a small number of
  // platforms), abort the test.
  if (!SwitchToFrench()) {
    LOG(ERROR) << "Failed switching to French language, aborting test.";
    return;
  }

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Open a new tab.
  content::WebContents* active_tab =
      OpenNewTab(browser(), GURL(chrome::kChromeUINewTabURL));

  // Verify that the NTP is in French.
  EXPECT_EQ(base::ASCIIToUTF16("Nouvel onglet"), active_tab->GetTitle());
}

// In contrast to LocalNTPTest, this one doesn't set up any special NTP
// wrangling. It just turns on the local NTP.
class LocalNTPSmokeTest : public InProcessBrowserTest {
 public:
  LocalNTPSmokeTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    cmdline->AppendSwitchASCII(switches::kEnableFeatures, "UseGoogleLocalNtp");
  }

  void SetUserSelectedDefaultSearchProvider(const std::string& base_url) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(base_url));
    data.SetKeyword(base::UTF8ToUTF16(base_url));
    data.SetURL(base_url + "url?bar={searchTerms}");

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPSmokeTest, GoogleNTPLoadsWithoutError) {
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

IN_PROC_BROWSER_TEST_F(LocalNTPSmokeTest, NonGoogleNTPLoadsWithoutError) {
  SetUserSelectedDefaultSearchProvider("https://www.example.com");

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

IN_PROC_BROWSER_TEST_F(LocalNTPSmokeTest, FrenchGoogleNTPLoadsWithoutError) {
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

// A simple fake implementation of OneGoogleBarFetcher that immediately returns
// a pre-configured OneGoogleBarData, which is null by default.
class FakeOneGoogleBarFetcher : public OneGoogleBarFetcher {
 public:
  void Fetch(OneGoogleCallback callback) override {
    std::move(callback).Run(one_google_bar_data_);
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

 protected:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    cmdline->AppendSwitchASCII(switches::kEnableFeatures,
                               "UseGoogleLocalNtp,OneGoogleBarOnLocalNtp");
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&LocalNTPOneGoogleBarSmokeTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  FakeOneGoogleBarFetcher* one_google_bar_fetcher() {
    return static_cast<FakeOneGoogleBarFetcher*>(
        OneGoogleBarServiceFactory::GetForProfile(browser()->profile())
            ->fetcher_for_testing());
  }

 private:
  static std::unique_ptr<KeyedService> CreateOneGoogleBarService(
      content::BrowserContext* context) {
    SigninManagerBase* signin_manager = SigninManagerFactory::GetForProfile(
        Profile::FromBrowserContext(context));
    return base::MakeUnique<OneGoogleBarService>(
        signin_manager, base::MakeUnique<FakeOneGoogleBarFetcher>());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    OneGoogleBarServiceFactory::GetInstance()->SetTestingFactory(
        context, &LocalNTPOneGoogleBarSmokeTest::CreateOneGoogleBarService);
  }

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

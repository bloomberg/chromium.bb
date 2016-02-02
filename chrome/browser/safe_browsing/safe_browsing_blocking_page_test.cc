// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject known-
// threat urls.  It then uses a real browser to go to these urls, and sends
// "goback" or "proceed" commands and verifies they work.

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/local_database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/threat_details.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/test/url_request/url_request_mock_http_job.h"

using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::BrowserThread;
using content::InterstitialPage;
using content::NavigationController;
using content::RenderFrameHost;
using content::WebContents;

namespace safe_browsing {

namespace {

const char kEmptyPage[] = "empty.html";
const char kMalwarePage[] = "safe_browsing/malware.html";
const char kCrossSiteMalwarePage[] = "safe_browsing/malware2.html";
const char kMalwareIframe[] = "safe_browsing/malware_iframe.html";
const char kCrossSiteIframeUrl[] = "http://example.com/cross_site_iframe.html";
const char kUnrelatedUrl[] = "https://www.google.com";

// A SafeBrowsingDatabaseManager class that allows us to inject the malicious
// URLs.
class FakeSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager() {}

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  bool CheckBrowseUrl(const GURL& gurl, Client* client) override {
    if (badurls[gurl.spec()] == SB_THREAT_TYPE_SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                   this, gurl, client));
    return false;
  }

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    std::vector<SBThreatType> expected_threats;
    // TODO(nparker): Remove ref to LocalSafeBrowsingDatabase by calling
    // client->OnCheckBrowseUrlResult(..) directly.
    expected_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
    expected_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
    expected_threats.push_back(SB_THREAT_TYPE_URL_UNWANTED);
    LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
        std::vector<GURL>(1, gurl),
        std::vector<SBFullHash>(),
        client,
        MALWARE,
        expected_threats);
    sb_check.url_results[0] = badurls[gurl.spec()];
    sb_check.OnSafeBrowsingResult();
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    badurls[url.spec()] = threat_type;
  }

  // These are called when checking URLs, so we implement them.
  bool IsSupported() const override { return true; }
  bool ChecksAreAlwaysAsync() const override { return false; }
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override {
    return true;
  }

  // Called during startup, so must not check-fail.
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override {
    return true;
  }

  safe_browsing::ThreatSource GetThreatSource() const override {
    return safe_browsing::ThreatSource::LOCAL_PVER3;
  }

 private:
  ~FakeSafeBrowsingDatabaseManager() override {}

  base::hash_map<std::string, SBThreatType> badurls;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// A SafeBrowingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager :  public SafeBrowsingUIManager {
 public:
  explicit FakeSafeBrowsingUIManager(SafeBrowsingService* service)
      : SafeBrowsingUIManager(service), threat_details_done_(false) {}

  // Overrides SafeBrowsingUIManager
  void SendSerializedThreatDetails(const std::string& serialized) override {
    // Notify the UI thread that we got a report.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeSafeBrowsingUIManager::OnThreatDetailsDone, this,
                   serialized));
  }

  void OnThreatDetailsDone(const std::string& serialized) {
    if (threat_details_done_) {
      return;
    }
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    report_ = serialized;

    EXPECT_FALSE(threat_details_done_callback_.is_null());
    if (!threat_details_done_callback_.is_null()) {
      threat_details_done_callback_.Run();
      threat_details_done_callback_ = base::Closure();
    }
    threat_details_done_ = true;
  }

  void set_threat_details_done_callback(const base::Closure& callback) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(threat_details_done_callback_.is_null());
    threat_details_done_callback_ = callback;
  }

  std::string GetReport() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return report_;
  }

 protected:
  ~FakeSafeBrowsingUIManager() override {}

 private:
  std::string report_;
  base::Closure threat_details_done_callback_;
  bool threat_details_done_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService()
      : fake_database_manager_(),
        fake_ui_manager_() { }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  FakeSafeBrowsingDatabaseManager* fake_database_manager() {
    return fake_database_manager_;
  }
  // Returned pointer has the same lifespan as the ui_manager_ refcounted
  // object.
  FakeSafeBrowsingUIManager* fake_ui_manager() {
    return fake_ui_manager_;
  }

 protected:
  ~FakeSafeBrowsingService() override {}

  SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    fake_database_manager_ = new FakeSafeBrowsingDatabaseManager();
    return fake_database_manager_;
  }

  SafeBrowsingUIManager* CreateUIManager() override {
    fake_ui_manager_ = new FakeSafeBrowsingUIManager(this);
    return fake_ui_manager_;
  }

  SafeBrowsingProtocolManagerDelegate* GetProtocolManagerDelegate() override {
    // Our SafeBrowsingDatabaseManager doesn't implement this delegate.
    return NULL;
  }

 private:
  FakeSafeBrowsingDatabaseManager* fake_database_manager_;
  FakeSafeBrowsingUIManager* fake_ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() :
      most_recent_service_(NULL) { }
  ~TestSafeBrowsingServiceFactory() override {}

  SafeBrowsingService* CreateSafeBrowsingService() override {
    most_recent_service_ =  new FakeSafeBrowsingService();
    return most_recent_service_;
  }

  FakeSafeBrowsingService* most_recent_service() const {
    return most_recent_service_;
  }

 private:
  FakeSafeBrowsingService* most_recent_service_;
};

}  // namespace

class TestThreatDetailsFactory : public ThreatDetailsFactory {
 public:
  TestThreatDetailsFactory() : details_() {}
  ~TestThreatDetailsFactory() override {}

  ThreatDetails* CreateThreatDetails(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) override {
    details_ = new ThreatDetails(delegate, web_contents, unsafe_resource);
    return details_;
  }

  ThreatDetails* get_details() { return details_; }

 private:
  ThreatDetails* details_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                               WebContents* web_contents,
                               const GURL& main_frame_url,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(manager,
                                 web_contents,
                                 main_frame_url,
                                 unsafe_resources),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    threat_details_proceed_delay_ms_ = 100;
  }

  ~TestSafeBrowsingBlockingPage() override {
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    base::MessageLoopForUI::current()->QuitWhenIdle();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

  // InterstitialPageDelegate methods:
  void CommandReceived(const std::string& command) override {
    SafeBrowsingBlockingPage::CommandReceived(command);
  }
  void OnProceed() override { SafeBrowsingBlockingPage::OnProceed(); }
  void OnDontProceed() override { SafeBrowsingBlockingPage::OnDontProceed(); }

 private:
  bool wait_for_delete_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                            main_frame_url, unsafe_resources);
  }
};

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<testing::tuple<SBThreatType, bool>> {
 public:
  enum Visibility {
    VISIBILITY_ERROR = -1,
    HIDDEN = 0,
    VISIBLE = 1
  };

  SafeBrowsingBlockingPageBrowserTest() {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    SafeBrowsingService::RegisterFactory(NULL);
    ThreatDetails::RegisterFactory(NULL);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (testing::get<1>(GetParam()))
      content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->safe_browsing_service());

    ASSERT_TRUE(service);
    service->fake_database_manager()->SetURLThreatType(url, threat_type);
  }

  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  GURL SetupWarningAndNavigate() {
    GURL url = net::URLRequestMockHTTPJob::GetMockUrl(kEmptyPage);
    SetURLThreatType(url, testing::get<0>(GetParam()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  // Adds two safebrowsing threat results to the fake safebrowsing service,
  // navigates to a page with an iframe containing the threat site, and another
  // cross site iframe containing another threat site, and returns the url of
  // the parent page.
  GURL SetupThreatIframeWarningAndNavigate() {
    GURL url = net::URLRequestMockHTTPJob::GetMockUrl(kCrossSiteMalwarePage);
    GURL iframe_url = net::URLRequestMockHTTPJob::GetMockUrl(kMalwareIframe);
    GURL cross_site_url(kCrossSiteIframeUrl);
    SetURLThreatType(iframe_url, testing::get<0>(GetParam()));
    SetURLThreatType(cross_site_url, testing::get<0>(GetParam()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  void SendCommand(
      security_interstitials::SecurityInterstitialCommands command) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // We use InterstitialPage::GetInterstitialPage(tab) instead of
    // tab->GetInterstitialPage() because the tab doesn't have a pointer
    // to its interstital page until it gets a command from the renderer
    // that it has indeed displayed it -- and this sometimes happens after
    // NavigateToURL returns.
    SafeBrowsingBlockingPage* interstitial_page =
        static_cast<SafeBrowsingBlockingPage*>(
            InterstitialPage::GetInterstitialPage(contents)->
                GetDelegateForTesting());
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
    interstitial_page->CommandReceived(base::IntToString(command));
  }

  void AssertNoInterstitial(bool wait_for_delete) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (contents->ShowingInterstitialPage() && wait_for_delete) {
      // We'll get notified when the interstitial is deleted.
      TestSafeBrowsingBlockingPage* page =
          static_cast<TestSafeBrowsingBlockingPage*>(
              contents->GetInterstitialPage()->GetDelegateForTesting());
      ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
                page->GetTypeForTesting());
      page->WaitForDelete();
    }

    // Can't use InterstitialPage::GetInterstitialPage() because that
    // gets updated after the TestSafeBrowsingBlockingPage destructor
    ASSERT_FALSE(contents->ShowingInterstitialPage());
  }

  bool YesInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    return interstitial_page != NULL;
  }

  void SetReportSentCallback(const base::Closure& callback) {
    factory_.most_recent_service()
        ->fake_ui_manager()
        ->set_threat_details_done_callback(callback);
  }

  std::string GetReportSent() {
    return factory_.most_recent_service()->fake_ui_manager()->GetReport();
  }

  void MalwareRedirectCancelAndProceed(const std::string& open_function) {
    GURL load_url = net::URLRequestMockHTTPJob::GetMockUrl(
        "safe_browsing/interstitial_cancel.html");
    GURL malware_url = net::URLRequestMockHTTPJob::GetMockUrl(kMalwarePage);
    SetURLThreatType(malware_url, testing::get<0>(GetParam()));

    // Load the test page.
    ui_test_utils::NavigateToURL(browser(), load_url);
    // Trigger the safe browsing interstitial page via a redirect in
    // "openWin()".
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        GURL("javascript:" + open_function + "()"),
        CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForInterstitialAttach(contents);
    // Cancel the redirect request while interstitial page is open.
    browser()->tab_strip_model()->ActivateTabAt(0, true);
    ui_test_utils::NavigateToURL(browser(), GURL("javascript:stopWin()"));
    browser()->tab_strip_model()->ActivateTabAt(1, true);
    // Simulate the user clicking "proceed", there should be no crash.  Since
    // clicking proceed may do nothing (see comment in RedirectCanceled
    // below, and crbug.com/76460), we use SendCommand to trigger the callback
    // directly rather than using ClickAndWaitForDetach since there might not
    // be a notification to wait for.
    SendCommand(security_interstitials::CMD_PROCEED);
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    InterstitialPage* interstitial = InterstitialPage::GetInterstitialPage(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (!interstitial)
      return NULL;
    return interstitial->GetMainFrame();
  }

  bool WaitForReady() {
    InterstitialPage* interstitial = InterstitialPage::GetInterstitialPage(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (!interstitial)
      return false;
    return content::WaitForRenderFrameReady(interstitial->GetMainFrame());
  }

  Visibility GetVisibility(const std::string& node_id) {
    content::RenderFrameHost* rfh = GetRenderFrameHost();
    if (!rfh)
      return VISIBILITY_ERROR;

    scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
        rfh, "var node = document.getElementById('" + node_id +
                 "');\n"
                 "if (node)\n"
                 "   node.offsetWidth > 0 && node.offsetHeight > 0;"
                 "else\n"
                 "  'node not found';\n");
    if (!value.get())
      return VISIBILITY_ERROR;

    bool result = false;
    if (!value->GetAsBoolean(&result))
      return VISIBILITY_ERROR;

    return result ? VISIBLE : HIDDEN;
  }

  bool Click(const std::string& node_id) {
    content::RenderFrameHost* rfh = GetRenderFrameHost();
    if (!rfh)
      return false;
    // We don't use ExecuteScriptAndGetValue for this one, since clicking
    // the button/link may navigate away before the injected javascript can
    // reply, hanging the test.
    rfh->ExecuteJavaScriptForTests(base::ASCIIToUTF16(
        "document.getElementById('" + node_id + "').click();\n"));
    return true;
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    // We wait for interstitial_detached rather than nav_entry_committed, as
    // going back from a main-frame safe browsing interstitial page will not
    // cause a nav entry committed event.
    if (!Click(node_id))
      return false;
    content::WaitForInterstitialDetach(
        browser()->tab_strip_model()->GetActiveWebContents());
    return true;
  }

  void TestReportingDisabledAndDontProceed(const GURL& url) {
    SetURLThreatType(url, testing::get<0>(GetParam()));
    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_TRUE(WaitForReady());

    EXPECT_EQ(HIDDEN, GetVisibility("extended-reporting-opt-in"));
    EXPECT_EQ(HIDDEN, GetVisibility("opt-in-checkbox"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("help-link"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));

    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
    AssertNoInterstitial(false);          // Assert the interstitial is gone
    EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
              browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::Resource& actual_resource,
      const std::string& expected_url,
      const std::string& expected_parent,
      int expected_child_size,
      const std::string& expected_tag_name) {
    EXPECT_EQ(expected_url, actual_resource.url());
    // Finds the parent url by comparing resource ids.
    for (auto resource : report.resources()) {
      if (actual_resource.parent_id() == resource.id()) {
        EXPECT_EQ(expected_parent, resource.url());
        break;
      }
    }
    EXPECT_EQ(expected_child_size, actual_resource.child_ids_size());
    EXPECT_EQ(expected_tag_name, actual_resource.tag_name());
  }

 protected:
  TestThreatDetailsFactory details_factory_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageBrowserTest);
};

// TODO(linux_aura) https://crbug.com/163931
// TODO(win_aura) https://crbug.com/154081
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_RedirectInIFrameCanceled DISABLED_RedirectInIFrameCanceled
#else
#define MAYBE_RedirectInIFrameCanceled RedirectInIFrameCanceled
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_RedirectInIFrameCanceled) {
  // 1. Test the case that redirect is a subresource.
  MalwareRedirectCancelAndProceed("openWinIFrame");
  // If the redirect was from subresource but canceled, "proceed" will continue
  // with the rest of resources.
  AssertNoInterstitial(true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, RedirectCanceled) {
  // 2. Test the case that redirect is the only resource.
  MalwareRedirectCancelAndProceed("openWin");
  // Clicking proceed won't do anything if the main request is cancelled
  // already.  See crbug.com/76460.
  EXPECT_TRUE(YesInterstitial());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, DontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  SetupWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, Proceed) {
  GURL url = SetupWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeDontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  SetupThreatIframeWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);  // Assert the interstitial is gone

  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeProceed) {
  GURL url = SetupThreatIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       IframeOptInAndReportThreatDetails) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats. This test uses malware as an example to verify
  // this reporting functionality.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Set up testing url containing iframe and cross site iframe.
  GURL url = SetupThreatIframeWarningAndNavigate();

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kSafeBrowsingExtendedReportingEnabled));
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    // Do some basic verification of report contents.
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(net::URLRequestMockHTTPJob::GetMockUrl(kMalwareIframe).spec(),
              report.url());
    std::vector<ClientSafeBrowsingReportRequest::Resource> resources;
    for (auto resource: report.resources()) {
      resources.push_back(resource);
    }
    // Sort resources based on their urls.
    std::sort(resources.begin(), resources.end(),
              [](const ClientSafeBrowsingReportRequest::Resource& a,
                 const ClientSafeBrowsingReportRequest::Resource& b) -> bool {
                return a.url() < b.url();
              });
    ASSERT_EQ(3U, resources.size());
    VerifyResource(
        report, resources[0], kCrossSiteIframeUrl,
        net::URLRequestMockHTTPJob::GetMockUrl(kCrossSiteMalwarePage).spec(), 0,
        "IFRAME");
    VerifyResource(
        report, resources[1],
        net::URLRequestMockHTTPJob::GetMockUrl(kCrossSiteMalwarePage).spec(),
        net::URLRequestMockHTTPJob::GetMockUrl(kCrossSiteMalwarePage).spec(), 2,
        "");
    VerifyResource(
        report, resources[2],
        net::URLRequestMockHTTPJob::GetMockUrl(kMalwareIframe).spec(),
        url.spec(),  // kCrossSiteMalwarePage
        0, "IFRAME");
  }
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MainFrameBlockedShouldHaveNoDOMDetailsWhenDontProceed) {
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMalwarePage is not the page flagged as malware in this
  // test.)
  GURL safe_url(net::URLRequestMockHTTPJob::GetMockUrl(kMalwarePage));
  ui_test_utils::NavigateToURL(browser(), safe_url);

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  GURL url = SetupWarningAndNavigate();

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kSafeBrowsingExtendedReportingEnabled));
  EXPECT_EQ(safe_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageBrowserTest,
    MainFrameBlockedShouldHaveNoDOMDetailsWhenProceeding) {
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMalwarePage is not the page flagged as malware in this
  // test.)
  ui_test_utils::NavigateToURL(
      browser(), net::URLRequestMockHTTPJob::GetMockUrl(kMalwarePage));

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  GURL url = SetupWarningAndNavigate();

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Proceed through the warning.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
              prefs::kSafeBrowsingExtendedReportingEnabled));
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the unsafe site.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ProceedDisabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  SendCommand(security_interstitials::CMD_PROCEED);

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial(true);
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Verifies that the reporting checkbox is hidden on non-HTTP pages.
// TODO(mattm): Should also verify that no report is sent, but there isn't a
// good way to do that in the current design.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ReportingDisabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  TestReportingDisabledAndDontProceed(
      net::URLRequestMockHTTPJob::GetMockHttpsUrl(kEmptyPage));
}

// Verifies that the reporting checkbox is hidden when opt-in is
// disabled by policy.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       ReportingDisabledByPolicy) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);

  TestReportingDisabledAndDontProceed(
      net::URLRequestMockHTTPJob::GetMockUrl(kEmptyPage));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, LearnMore) {
  SetupWarningAndNavigate();
  EXPECT_TRUE(ClickAndWaitForDetach("help-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ(
      testing::get<0>(GetParam()) == SB_THREAT_TYPE_URL_PHISHING
          ? "/transparencyreport/safebrowsing/"
          : "/safebrowsing/diagnostic",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_DontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (https://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests)) {
    return;
  }
#endif

  base::HistogramTester histograms;
  std::string prefix;
  SBThreatType threat_type = testing::get<0>(GetParam());
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE)
    prefix = "malware";
  else if (threat_type == SB_THREAT_TYPE_URL_PHISHING)
    prefix = "phishing";
  else if (threat_type == SB_THREAT_TYPE_URL_UNWANTED)
    prefix = "harmful";
  else
    NOTREACHED();
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";

  // TODO(nparker): Check for *.from_device as well.

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  SetupWarningAndNavigate();
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_Proceed) {
  base::HistogramTester histograms;
  std::string prefix;
  SBThreatType threat_type = testing::get<0>(GetParam());
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE)
    prefix = "malware";
  else if (threat_type == SB_THREAT_TYPE_URL_PHISHING)
    prefix = "phishing";
  else if (threat_type == SB_THREAT_TYPE_URL_UNWANTED)
    prefix = "harmful";
  else
    NOTREACHED();
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  GURL url = SetupWarningAndNavigate();
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, WhitelistRevisit) {
  GURL url = SetupWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Unrelated pages should not be whitelisted now.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The whitelisted page should remain whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  AssertNoInterstitial(false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       WhitelistIframeRevisit) {
  GURL url = SetupThreatIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Unrelated pages should not be whitelisted now.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The whitelisted page should remain whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  AssertNoInterstitial(false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, WhitelistUnsaved) {
  GURL url = SetupWarningAndNavigate();

  // Navigate without making a decision.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The non-whitelisted page should now show an interstitial.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WaitForReady());
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
}

INSTANTIATE_TEST_CASE_P(
    SafeBrowsingBlockingPageBrowserTestWithThreatTypeAndIsolationSetting,
    SafeBrowsingBlockingPageBrowserTest,
    testing::Combine(
        testing::Values(SB_THREAT_TYPE_URL_MALWARE,  // Threat types
                        SB_THREAT_TYPE_URL_PHISHING,
                        SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

// Test that SafeBrowsingBlockingPage properly decodes IDN URLs that are
// displayed.
class SafeBrowsingBlockingPageIDNTest
    : public SecurityInterstitialIDNTest,
      public testing::WithParamInterface<testing::tuple<bool, SBThreatType>> {
 protected:
  // SecurityInterstitialIDNTest implementation
  SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    const bool is_subresource = testing::get<0>(GetParam());

    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    SafeBrowsingBlockingPage::UnsafeResource resource;

    resource.url = request_url;
    resource.is_subresource = is_subresource;
    resource.threat_type = testing::get<1>(GetParam());
    resource.render_process_host_id = contents->GetRenderProcessHost()->GetID();
    resource.render_frame_id = contents->GetMainFrame()->GetRoutingID();
    resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;

    return SafeBrowsingBlockingPage::CreateBlockingPage(
        sb_service->ui_manager().get(), contents,
        is_subresource ? GURL("http://mainframe.example.com/") : request_url,
        resource);
  }
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageIDNTest,
                       SafeBrowsingBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

INSTANTIATE_TEST_CASE_P(
    SafeBrowsingBlockingPageIDNTestWithThreatType,
    SafeBrowsingBlockingPageIDNTest,
    testing::Combine(testing::Values(false, true),
                     testing::Values(SB_THREAT_TYPE_URL_MALWARE,
                                     SB_THREAT_TYPE_URL_PHISHING,
                                     SB_THREAT_TYPE_URL_UNWANTED)));

}  // namespace safe_browsing

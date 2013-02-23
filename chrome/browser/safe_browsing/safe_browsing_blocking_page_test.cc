// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject
// malware and phishing urls.  It then uses a real browser to go to
// these urls, and sends "goback" or "proceed" commands and verifies
// they work.

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;
using content::InterstitialPage;
using content::NavigationController;
using content::WebContents;

namespace {

const char kEmptyPage[] = "files/empty.html";
const char kMalwarePage[] = "files/safe_browsing/malware.html";
const char kMalwareIframe[] = "files/safe_browsing/malware_iframe.html";

// A SafeBrowsingDatabaseManager class that allows us to inject the malicious
// URLs.
class FakeSafeBrowsingDatabaseManager :  public SafeBrowsingDatabaseManager {
 public:
  explicit FakeSafeBrowsingDatabaseManager(SafeBrowsingService* service)
      : SafeBrowsingDatabaseManager(service) { }

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  virtual bool CheckBrowseUrl(const GURL& gurl, Client* client) OVERRIDE {
    if (badurls[gurl.spec()] == SB_THREAT_TYPE_SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                   this, gurl, client));
    return false;
  }

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    SafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
        std::vector<GURL>(1, gurl),
        std::vector<SBFullHash>(),
        client,
        safe_browsing_util::MALWARE);
    sb_check.url_results[0] = badurls[gurl.spec()];
    client->OnSafeBrowsingResult(sb_check);
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    badurls[url.spec()] = threat_type;
  }

 private:
  virtual ~FakeSafeBrowsingDatabaseManager() {}

  base::hash_map<std::string, SBThreatType> badurls;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// A SafeBrowingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager :  public SafeBrowsingUIManager {
 public:
  explicit FakeSafeBrowsingUIManager(SafeBrowsingService* service) :
      SafeBrowsingUIManager(service) { }

  // Overrides SafeBrowsingUIManager
  virtual void SendSerializedMalwareDetails(
      const std::string& serialized) OVERRIDE {
    reports_.push_back(serialized);
    // Notify the UI thread that we got a report.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeSafeBrowsingUIManager::OnMalwareDetailsDone, this));
  }

  void OnMalwareDetailsDone() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoopForUI::current()->Quit();
  }

  std::string GetReport() {
    EXPECT_TRUE(reports_.size() == 1);
    return reports_[0];
  }

 protected:
  virtual ~FakeSafeBrowsingUIManager() { }

 private:
  std::vector<std::string> reports_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() { }

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
  virtual ~FakeSafeBrowsingService() { }

  virtual SafeBrowsingDatabaseManager* CreateDatabaseManager() OVERRIDE {
    fake_database_manager_ = new FakeSafeBrowsingDatabaseManager(this);
    return fake_database_manager_;
  }

  virtual SafeBrowsingUIManager* CreateUIManager() OVERRIDE {
    fake_ui_manager_ = new FakeSafeBrowsingUIManager(this);
    return fake_ui_manager_;
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
  virtual ~TestSafeBrowsingServiceFactory() { }

  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    most_recent_service_ =  new FakeSafeBrowsingService();
    return most_recent_service_;
  }

  FakeSafeBrowsingService* most_recent_service() const {
    return most_recent_service_;
  }

 private:
  FakeSafeBrowsingService* most_recent_service_;
};

// A MalwareDetails class lets us intercept calls from the renderer.
class FakeMalwareDetails : public MalwareDetails {
 public:
  FakeMalwareDetails(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource)
      : MalwareDetails(delegate, web_contents, unsafe_resource),
        got_dom_(false),
        waiting_(false) { }

  virtual void AddDOMDetails(
      const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params)
          OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    MalwareDetails::AddDOMDetails(params);

    // Notify the UI thread that we got the dom details.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&FakeMalwareDetails::OnDOMDetailsDone,
                                       this));
  }

  void WaitForDOM() {
    if (got_dom_) {
      LOG(INFO) << "Already got the dom details.";
      return;
    }
    // This condition might not trigger normally, but if you add a
    // sleep(1) in malware_dom_details it triggers :).
    waiting_ = true;
    LOG(INFO) << "Waiting for dom details.";
    content::RunMessageLoop();
    EXPECT_TRUE(got_dom_);
  }

 private:
  virtual ~FakeMalwareDetails() {}

  void OnDOMDetailsDone() {
    got_dom_ = true;
    if (waiting_) {
      MessageLoopForUI::current()->Quit();
    }
  }

  // Some logic to figure out if we should wait for the dom details or not.
  // These variables should only be accessed in the UI thread.
  bool got_dom_;
  bool waiting_;
};

class TestMalwareDetailsFactory : public MalwareDetailsFactory {
 public:
  TestMalwareDetailsFactory() { }
  virtual ~TestMalwareDetailsFactory() { }

  virtual MalwareDetails* CreateMalwareDetails(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) OVERRIDE {
    details_ = new FakeMalwareDetails(delegate, web_contents,
                                      unsafe_resource);
    return details_;
  }

  FakeMalwareDetails* get_details() {
    return details_;
  }

 private:
  FakeMalwareDetails* details_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPageV2 {
 public:
  TestSafeBrowsingBlockingPage(SafeBrowsingUIManager* manager,
                               WebContents* web_contents,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPageV2(manager, web_contents, unsafe_resources),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    malware_details_proceed_delay_ms_ = 100;
  }

  virtual ~TestSafeBrowsingBlockingPage() {
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    MessageLoopForUI::current()->Quit();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

 private:
  bool wait_for_delete_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  virtual ~TestSafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
          OVERRIDE {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                            unsafe_resources);
  }
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageTest : public InProcessBrowserTest {
 public:
  enum Visibility {
    VISIBILITY_ERROR = -1,
    HIDDEN = 0,
    VISIBLE = 1,
  };

  SafeBrowsingBlockingPageTest() {
  }

  virtual void SetUp() OVERRIDE {
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    MalwareDetails::RegisterFactory(&details_factory_);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    InProcessBrowserTest::TearDown();
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    SafeBrowsingService::RegisterFactory(NULL);
    MalwareDetails::RegisterFactory(NULL);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(test_server()->Start());
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->safe_browsing_service());

    ASSERT_TRUE(service);
    service->fake_database_manager()->SetURLThreatType(url, threat_type);
  }

  // Adds a safebrowsing result of type |threat_type| to the fake safebrowsing
  // service, navigates to that page, and returns the url.
  GURL SetupWarningAndNavigate(SBThreatType threat_type) {
    GURL url = test_server()->GetURL(kEmptyPage);
    SetURLThreatType(url, threat_type);

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  // Adds a safebrowsing malware result to the fake safebrowsing service,
  // navigates to a page with an iframe containing the malware site, and
  // returns the url of the parent page.
  GURL SetupMalwareIframeWarningAndNavigate() {
    GURL url = test_server()->GetURL(kMalwarePage);
    GURL iframe_url = test_server()->GetURL(kMalwareIframe);
    SetURLThreatType(iframe_url, SB_THREAT_TYPE_URL_MALWARE);

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady());
    return url;
  }

  void SendCommand(const std::string& command) {
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
    interstitial_page->CommandReceived(command);
  }

  void DontProceedThroughInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    ASSERT_TRUE(interstitial_page);
    interstitial_page->DontProceed();
  }

  void ProceedThroughInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = InterstitialPage::GetInterstitialPage(
        contents);
    ASSERT_TRUE(interstitial_page);
    interstitial_page->Proceed();
  }

  void AssertNoInterstitial(bool wait_for_delete) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (contents->ShowingInterstitialPage() && wait_for_delete) {
      // We'll get notified when the interstitial is deleted.
      TestSafeBrowsingBlockingPage* page =
          static_cast<TestSafeBrowsingBlockingPage*>(
              contents->GetInterstitialPage()->GetDelegateForTesting());
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

  void WaitForInterstitial() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WindowedNotificationObserver interstitial_observer(
        content::NOTIFICATION_INTERSTITIAL_ATTACHED,
        content::Source<WebContents>(contents));
    if (!InterstitialPage::GetInterstitialPage(contents))
      interstitial_observer.Wait();
  }

  void AssertReportSent() {
    // When a report is scheduled in the IO thread we should get notified.
    content::RunMessageLoop();

    std::string serialized = factory_.most_recent_service()->
        fake_ui_manager()->GetReport();

    safe_browsing::ClientMalwareReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));

    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
  }

  void MalwareRedirectCancelAndProceed(const std::string open_function) {
    GURL load_url = test_server()->GetURL(
        "files/safe_browsing/interstitial_cancel.html");
    GURL malware_url("http://localhost/files/safe_browsing/malware.html");
    SetURLThreatType(malware_url, SB_THREAT_TYPE_URL_MALWARE);

    // Load the test page.
    ui_test_utils::NavigateToURL(browser(), load_url);
    // Trigger the safe browsing interstitial page via a redirect in
    // "openWin()".
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        GURL("javascript:" + open_function + "()"),
        CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    WaitForInterstitial();
    // Cancel the redirect request while interstitial page is open.
    browser()->tab_strip_model()->ActivateTabAt(0, true);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        GURL("javascript:stopWin()"),
        CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    browser()->tab_strip_model()->ActivateTabAt(1, true);
    // Simulate the user clicking "proceed", there should be no crash.  Since
    // clicking proceed may do nothing (see comment in MalwareRedirectCanceled
    // below, and crbug.com/76460), we use SendCommand to trigger the callback
    // directly rather than using ClickAndWaitForDetach since there might not
    // be a notification to wait for.
    SendCommand("\"proceed\"");
  }

  content::RenderViewHost* GetRenderViewHost() {
    InterstitialPage* interstitial = InterstitialPage::GetInterstitialPage(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (!interstitial)
      return NULL;
    return interstitial->GetRenderViewHostForTesting();
  }

  bool WaitForReady() {
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return false;
    // Wait until all <script> tags have executed, including jstemplate.
    // TODO(joaodasilva): it would be nice to avoid the busy loop, though in
    // practice it spins at most once or twice.
    std::string ready_state;
    do {
      scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
          rvh, "document.readyState");
      if (!value.get() || !value->GetAsString(&ready_state))
        return false;
    } while (ready_state != "complete");
    return true;
  }

  Visibility GetVisibility(const std::string& node_id) {
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return VISIBILITY_ERROR;
    scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
        rvh,
        "var node = document.getElementById('" + node_id + "');\n"
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
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return false;
    // We don't use ExecuteScriptAndGetValue for this one, since clicking
    // the button/link may navigate away before the injected javascript can
    // reply, hanging the test.
    rvh->ExecuteJavascriptInWebFrame(
        string16(),
        ASCIIToUTF16("document.getElementById('" + node_id + "').click();\n"));
    return true;
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    // We wait for interstitial_detached rather than nav_entry_committed, as
    // going back from a main-frame malware interstitial page will not cause a
    // nav entry committed event.
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_INTERSTITIAL_DETACHED,
        content::Source<WebContents>(
            browser()->tab_strip_model()->GetActiveWebContents()));
    if (!Click(node_id))
      return false;
    observer.Wait();
    return true;
  }

 protected:
  TestMalwareDetailsFactory details_factory_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageTest);
};

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_MalwareRedirectInIFrameCanceled DISABLED_MalwareRedirectInIFrameCanceled
#else
#define MAYBE_MalwareRedirectInIFrameCanceled MalwareRedirectInIFrameCanceled
#endif
IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MAYBE_MalwareRedirectInIFrameCanceled) {
  // 1. Test the case that redirect is a subresource.
  MalwareRedirectCancelAndProceed("openWinIFrame");
  // If the redirect was from subresource but canceled, "proceed" will continue
  // with the rest of resources.
  AssertNoInterstitial(true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MalwareRedirectCanceled) {
  // 2. Test the case that redirect is the only resource.
  MalwareRedirectCancelAndProceed("openWin");
  // Clicking proceed won't do anything if the main request is cancelled
  // already.  See crbug.com/76460.
  EXPECT_TRUE(YesInterstitial());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareDontProceed) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_EQ(VISIBLE, GetVisibility("malware-icon"));
  EXPECT_EQ(HIDDEN, GetVisibility("subresource-icon"));
  EXPECT_EQ(HIDDEN, GetVisibility("phishing-icon"));
  EXPECT_EQ(VISIBLE, GetVisibility("check-report"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("report-error-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_TRUE(Click("see-more-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("report-error-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed"));

  EXPECT_TRUE(ClickAndWaitForDetach("back"));
  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(
      GURL(chrome::kAboutBlankURL),  // Back to "about:blank"
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareProceed) {
  GURL url = SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MalwareLearnMore) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_TRUE(ClickAndWaitForDetach("learn-more-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ(
      "/goodtoknow/online-safety/malware/",
       browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MalwareIframeDontProceed) {
  SetupMalwareIframeWarningAndNavigate();

  EXPECT_EQ(HIDDEN, GetVisibility("malware-icon"));
  EXPECT_EQ(VISIBLE, GetVisibility("subresource-icon"));
  EXPECT_EQ(HIDDEN, GetVisibility("phishing-icon"));
  EXPECT_EQ(VISIBLE, GetVisibility("check-report"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("report-error-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_TRUE(Click("see-more-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("report-error-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed"));

  EXPECT_TRUE(ClickAndWaitForDetach("back"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  EXPECT_EQ(
      GURL(chrome::kAboutBlankURL),  // Back to "about:blank"
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareIframeProceed) {
  GURL url = SetupMalwareIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MalwareIframeReportDetails) {
  GURL url = SetupMalwareIframeWarningAndNavigate();

  // If the DOM details from renderer did not already return, wait for them.
  details_factory_.get_details()->WaitForDOM();

  EXPECT_TRUE(Click("check-report"));

  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  AssertReportSent();
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the malware site.
IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, ProceedDisabled) {
  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_EQ(VISIBLE, GetVisibility("check-report"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-span"));
  EXPECT_TRUE(Click("see-more-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-span"));

  // The "proceed" command should go back instead, if proceeding is disabled.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  AssertNoInterstitial(true);
  EXPECT_EQ(
      GURL(chrome::kAboutBlankURL),  // Back to "about:blank"
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Verifies that the reporting checkbox is hidden on non-HTTP pages.
// TODO(mattm): Should also verify that no report is sent, but there isn't a
// good way to do that in the current design.
IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, ReportingDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingReportingEnabled, true);

  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS, net::TestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL(kEmptyPage);
  SetURLThreatType(url, SB_THREAT_TYPE_URL_MALWARE);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WaitForReady());

  EXPECT_EQ(HIDDEN, GetVisibility("check-report"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_TRUE(Click("see-more-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed"));

  EXPECT_TRUE(ClickAndWaitForDetach("back"));
  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(
      GURL(chrome::kAboutBlankURL),  // Back to "about:blank"
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingDontProceed) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_EQ(HIDDEN, GetVisibility("malware-icon"));
  EXPECT_EQ(HIDDEN, GetVisibility("subresource-icon"));
  EXPECT_EQ(VISIBLE, GetVisibility("phishing-icon"));
  EXPECT_EQ(HIDDEN, GetVisibility("check-report"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("report-error-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
  EXPECT_TRUE(Click("see-more-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("report-error-link"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed"));

  EXPECT_TRUE(ClickAndWaitForDetach("back"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone
  EXPECT_EQ(
      GURL(chrome::kAboutBlankURL),  // We are back to "about:blank".
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingProceed) {
  GURL url = SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingReportError) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_TRUE(ClickAndWaitForDetach("report-error-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the error reporting page.
  EXPECT_EQ(
      "/safebrowsing/report_error/",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingLearnMore) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_TRUE(ClickAndWaitForDetach("learn-more-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ(
      "/goodtoknow/online-safety/phishing/",
       browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

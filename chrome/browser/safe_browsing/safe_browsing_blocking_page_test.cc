// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject
// malware and phishing urls.  It then uses a real browser to go to
// these urls, and sends "goback" or "proceed" commands and verifies
// they work.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

// A SafeBrowingService class that allows us to inject the malicious URLs.
class FakeSafeBrowsingService :  public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() {}

  virtual ~FakeSafeBrowsingService() {}

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Overrides SafeBrowsingService::CheckBrowseUrl.
  virtual bool CheckBrowseUrl(const GURL& gurl, Client* client) {
    if (badurls[gurl.spec()] == SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &FakeSafeBrowsingService::OnCheckBrowseURLDone,
                          gurl, client));
    return false;
  }

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    SafeBrowsingService::SafeBrowsingCheck check;
    check.url.reset(new GURL(gurl));
    check.client = client;
    check.result = badurls[gurl.spec()];
    client->OnSafeBrowsingResult(check);
  }

  void AddURLResult(const GURL& url, UrlCheckResult checkresult) {
    badurls[url.spec()] = checkresult;
  }

  virtual void ReportMalwareDetails(scoped_refptr<MalwareDetails> details) {
    details_.push_back(details);
    // Notify the UI thread, that we got a report.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            NewRunnableMethod(this,
                            &FakeSafeBrowsingService::OnMalwareDetailsDone));
  }

  void OnMalwareDetailsDone() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoopForUI::current()->Quit();
  }

  std::list<scoped_refptr<MalwareDetails> >* GetDetails() {
    return &details_;
  }

  std::list<scoped_refptr<MalwareDetails> > details_;


 private:
  base::hash_map<std::string, UrlCheckResult> badurls;
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() { }
  virtual ~TestSafeBrowsingServiceFactory() { }

  virtual SafeBrowsingService* CreateSafeBrowsingService() {
    return new FakeSafeBrowsingService();
  }
};

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageTest : public InProcessBrowserTest,
                                     public SafeBrowsingService::Client {
 public:
  SafeBrowsingBlockingPageTest() {
  }

  virtual void SetUp() {
    SafeBrowsingService::RegisterFactory(&factory);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    InProcessBrowserTest::TearDown();
    SafeBrowsingService::RegisterFactory(NULL);
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ASSERT_TRUE(test_server()->Start());
  }

  // SafeBrowsingService::Client implementation.
  virtual void OnSafeBrowsingResult(
      const SafeBrowsingService::SafeBrowsingCheck& check) {
  }

  virtual void OnBlockingPageComplete(bool proceed) {
  }

  void AddURLResult(const GURL& url,
                    SafeBrowsingService::UrlCheckResult checkresult) {
    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->resource_dispatcher_host()->
            safe_browsing_service());

    ASSERT_TRUE(service);
    service->AddURLResult(url, checkresult);
  }

  void SendCommand(const std::string& command) {
    TabContents* contents = browser()->GetSelectedTabContents();
    SafeBrowsingBlockingPage* interstitial_page =
        static_cast<SafeBrowsingBlockingPage*>(
            contents->interstitial_page());
    ASSERT_TRUE(interstitial_page);
    interstitial_page->CommandReceived(command);
  }

  void DontProceedThroughInterstitial() {
    TabContents* contents = browser()->GetSelectedTabContents();
    InterstitialPage* interstitial_page = contents->interstitial_page();
    ASSERT_TRUE(interstitial_page);
    interstitial_page->DontProceed();
  }

  void ProceedThroughInterstitial() {
    TabContents* contents = browser()->GetSelectedTabContents();
    InterstitialPage* interstitial_page = contents->interstitial_page();
    ASSERT_TRUE(interstitial_page);
    interstitial_page->Proceed();
  }

  void AssertNoInterstitial() {
    TabContents* contents = browser()->GetSelectedTabContents();
    InterstitialPage* interstitial_page = contents->interstitial_page();
    ASSERT_FALSE(interstitial_page);
  }

  void WaitForNavigation() {
    NavigationController* controller =
        &browser()->GetSelectedTabContents()->controller();
    ui_test_utils::WaitForNavigation(controller);
  }

  void AssertReportSent() {
    // When a report is scheduled in the IO thread we should get notified.
    ui_test_utils::RunMessageLoop();

    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->resource_dispatcher_host()->
            safe_browsing_service());
    ASSERT_EQ(1u, service->GetDetails()->size());
  }

 private:
  TestSafeBrowsingServiceFactory factory;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageTest);
};

namespace {

const char kEmptyPage[] = "files/empty.html";
const char kMalwarePage[] = "files/safe_browsing/malware.html";
const char kMalwareIframe[] = "files/safe_browsing/malware_iframe.html";

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareDontProceed) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"takeMeBack\"");   // Simulate the user clicking "back"
  AssertNoInterstitial();   // Assert the interstitial is gone
  EXPECT_EQ(GURL(chrome::kAboutBlankURL),   // Back to "about:blank"
            browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareProceed) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"proceed\"");    // Simulate the user clicking "proceed"
  WaitForNavigation();    // Wait until we finish the navigation.
  AssertNoInterstitial();    // Assert the interstitial is gone.
  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingDontProceed) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_PHISHING);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"takeMeBack\"");   // Simulate the user clicking "proceed"
  AssertNoInterstitial();    // Assert the interstitial is gone
  EXPECT_EQ(GURL(chrome::kAboutBlankURL),  // We are back to "about:blank".
            browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingProceed) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_PHISHING);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"proceed\"");   // Simulate the user clicking "proceed".
  WaitForNavigation();    // Wait until we finish the navigation.
  AssertNoInterstitial();    // Assert the interstitial is gone
  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingReportError) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_PHISHING);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"reportError\"");   // Simulate the user clicking "report error"
  WaitForNavigation();    // Wait until we finish the navigation.
  AssertNoInterstitial();    // Assert the interstitial is gone

  // We are in the error reporting page.
  EXPECT_EQ("/safebrowsing/report_error/",
            browser()->GetSelectedTabContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, PhishingLearnMore) {
  GURL url = test_server()->GetURL(kEmptyPage);
  AddURLResult(url, SafeBrowsingService::URL_PHISHING);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"learnMore\"");   // Simulate the user clicking "learn more"
  WaitForNavigation();    // Wait until we finish the navigation.
  AssertNoInterstitial();    // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ("/support/bin/answer.py",
            browser()->GetSelectedTabContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareIframeDontProceed) {
  GURL url = test_server()->GetURL(kMalwarePage);
  GURL iframe_url = test_server()->GetURL(kMalwareIframe);
  AddURLResult(iframe_url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"takeMeBack\"");    // Simulate the user clicking "back"
  AssertNoInterstitial();  // Assert the interstitial is gone

  EXPECT_EQ(GURL(chrome::kAboutBlankURL),    // Back to "about:blank"
            browser()->GetSelectedTabContents()->GetURL());
}

// Crashy, http://crbug.com/68834.
IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       DISABLED_MalwareIframeProceed) {
  GURL url = test_server()->GetURL(kMalwarePage);
  GURL iframe_url = test_server()->GetURL(kMalwareIframe);
  AddURLResult(iframe_url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"proceed\"");   // Simulate the user clicking "proceed"
  AssertNoInterstitial();    // Assert the interstitial is gone

  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest,
                       MalwareIframeReportDetails) {
  GURL url = test_server()->GetURL(kMalwarePage);
  GURL iframe_url = test_server()->GetURL(kMalwareIframe);
  AddURLResult(iframe_url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"doReport\"");  // Simulate the user checking the checkbox.
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingReportingEnabled));

  SendCommand("\"proceed\"");  // Simulate the user clicking "back"
  AssertNoInterstitial();  // Assert the interstitial is gone

  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
  AssertReportSent();
}

}  // namespace

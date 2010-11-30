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
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/browser.h"
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
  // Overrides SafeBrowsingService::CheckUrl.
  virtual bool CheckUrl(const GURL& gurl, Client* client) {
    const std::string& url = gurl.spec();
    if (badurls[url] == URL_SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &FakeSafeBrowsingService::OnCheckDone,
                          url, client));
    return false;
  }

  void OnCheckDone(std::string url, Client* client) {
    client->OnUrlCheckResult(GURL(url), badurls[url]);
  }

  void AddURLResult(const GURL& url, UrlCheckResult checkresult) {
    badurls[url.spec()] = checkresult;
  }

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
  virtual void OnUrlCheckResult(const GURL& url,
                                SafeBrowsingService::UrlCheckResult result) {
  }
  virtual void OnBlockingPageComplete(bool proceed) {
  }

  void AddURLResult(const GURL& url,
                    SafeBrowsingService::UrlCheckResult checkresult) {
    FakeSafeBrowsingService* service =
        static_cast<FakeSafeBrowsingService*>(
            g_browser_process->resource_dispatcher_host()->
            safe_browsing_service());

    ASSERT_TRUE(service != NULL);
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

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageTest, MalwareIframeProceed) {
  GURL url = test_server()->GetURL(kMalwarePage);
  GURL iframe_url = test_server()->GetURL(kMalwareIframe);
  AddURLResult(iframe_url, SafeBrowsingService::URL_MALWARE);

  ui_test_utils::NavigateToURL(browser(), url);

  SendCommand("\"proceed\"");   // Simulate the user clicking "proceed"
  AssertNoInterstitial();    // Assert the interstitial is gone

  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
}

}  // namespace

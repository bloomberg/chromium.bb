// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject
// malware and phishing urls.  It then uses a real browser to go to
// these urls, and sends "goback" or "proceed" commands and verifies
// they work.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
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

class InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(content::WebContents* web_contents,
                       const base::Closure& attach_callback,
                       const base::Closure& detach_callback)
      : WebContentsObserver(web_contents),
        attach_callback_(attach_callback),
        detach_callback_(detach_callback) {
  }

  virtual void DidAttachInterstitialPage() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    attach_callback_.Run();
  }

  virtual void DidDetachInterstitialPage() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    detach_callback_.Run();
  }

 private:
  base::Closure attach_callback_;
  base::Closure detach_callback_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

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
    std::vector<SBThreatType> expected_threats;
    expected_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
    expected_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
    SafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
        std::vector<GURL>(1, gurl),
        std::vector<SBFullHash>(),
        client,
        safe_browsing_util::MALWARE,
        expected_threats);
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
    // Notify the UI thread that we got a report.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FakeSafeBrowsingUIManager::OnMalwareDetailsDone,
                   this,
                   serialized));
  }

  void OnMalwareDetailsDone(const std::string& serialized) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    report_ = serialized;

    EXPECT_FALSE(malware_details_done_callback_.is_null());
    if (!malware_details_done_callback_.is_null()) {
      malware_details_done_callback_.Run();
      malware_details_done_callback_ = base::Closure();
    }
  }

  void set_malware_details_done_callback(const base::Closure& callback) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(malware_details_done_callback_.is_null());
    malware_details_done_callback_ = callback;
  }

  std::string GetReport() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return report_;
  }

 protected:
  virtual ~FakeSafeBrowsingUIManager() { }

 private:
  std::string report_;
  base::Closure malware_details_done_callback_;

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
      base::MessageLoopForUI::current()->Quit();
    }
  }

  // Some logic to figure out if we should wait for the dom details or not.
  // These variables should only be accessed in the UI thread.
  bool got_dom_;
  bool waiting_;
};

class TestMalwareDetailsFactory : public MalwareDetailsFactory {
 public:
  TestMalwareDetailsFactory() : details_() { }
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
class TestSafeBrowsingBlockingPageV2 : public SafeBrowsingBlockingPageV2 {
 public:
  TestSafeBrowsingBlockingPageV2(SafeBrowsingUIManager* manager,
                                 WebContents* web_contents,
                                 const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPageV2(manager, web_contents, unsafe_resources),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    malware_details_proceed_delay_ms_ = 100;
  }

  virtual ~TestSafeBrowsingBlockingPageV2() {
    LOG(INFO) << __FUNCTION__;
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    base::MessageLoopForUI::current()->Quit();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    LOG(INFO) << __FUNCTION__;
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

  // InterstitialPageDelegate methods:
  virtual void CommandReceived(const std::string& command) OVERRIDE {
    LOG(INFO) << __FUNCTION__ << " " << command;
    SafeBrowsingBlockingPageV2::CommandReceived(command);
  }
  virtual void OnProceed() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    SafeBrowsingBlockingPageV2::OnProceed();
  }
  virtual void OnDontProceed() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    SafeBrowsingBlockingPageV2::OnDontProceed();
  }

 private:
  bool wait_for_delete_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPageV3 : public SafeBrowsingBlockingPageV3 {
 public:
  TestSafeBrowsingBlockingPageV3(SafeBrowsingUIManager* manager,
                                 WebContents* web_contents,
                                 const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPageV3(manager, web_contents, unsafe_resources),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    malware_details_proceed_delay_ms_ = 100;
  }

  virtual ~TestSafeBrowsingBlockingPageV3() {
    LOG(INFO) << __FUNCTION__;
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    base::MessageLoopForUI::current()->Quit();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    LOG(INFO) << __FUNCTION__;
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

  // InterstitialPageDelegate methods:
  virtual void CommandReceived(const std::string& command) OVERRIDE {
    LOG(INFO) << __FUNCTION__ << " " << command;
    SafeBrowsingBlockingPageV3::CommandReceived(command);
  }
  virtual void OnProceed() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    SafeBrowsingBlockingPageV3::OnProceed();
  }
  virtual void OnDontProceed() OVERRIDE {
    LOG(INFO) << __FUNCTION__;
    SafeBrowsingBlockingPageV3::OnDontProceed();
  }

 private:
  bool wait_for_delete_;
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() : version_(2) { }
  virtual ~TestSafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* delegate,
      WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
          OVERRIDE {
    if (version_ == 3) {
      return new TestSafeBrowsingBlockingPageV3(delegate, web_contents,
                                                unsafe_resources);
    }
    return new TestSafeBrowsingBlockingPageV2(delegate, web_contents,
                                              unsafe_resources);
  }

  void SetTestVersion(int version) {
    version_ = version;
  }

 private:
  int version_;
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<int> {
 public:
  enum Visibility {
    VISIBILITY_ERROR = -1,
    HIDDEN = 0,
    VISIBLE = 1
  };

  SafeBrowsingBlockingPageBrowserTest() {
  }

  virtual void SetUp() OVERRIDE {
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    blocking_page_factory_.SetTestVersion(GetParam());
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

    LOG(INFO) << "navigating... " << url.spec();
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
      if (GetParam() == 3) {
        TestSafeBrowsingBlockingPageV3* page =
            static_cast<TestSafeBrowsingBlockingPageV3*>(
                contents->GetInterstitialPage()->GetDelegateForTesting());
        page->WaitForDelete();
      } else {
        TestSafeBrowsingBlockingPageV2* page =
            static_cast<TestSafeBrowsingBlockingPageV2*>(
                contents->GetInterstitialPage()->GetDelegateForTesting());
        page->WaitForDelete();
      }
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
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    InterstitialObserver observer(contents,
                                  loop_runner->QuitClosure(),
                                  base::Closure());
    if (!InterstitialPage::GetInterstitialPage(contents))
      loop_runner->Run();
  }

  void SetReportSentCallback(const base::Closure& callback) {
    LOG(INFO) << __FUNCTION__;
    factory_.most_recent_service()
        ->fake_ui_manager()
        ->set_malware_details_done_callback(callback);
  }

  std::string GetReportSent() {
    LOG(INFO) << __FUNCTION__;
    return factory_.most_recent_service()->fake_ui_manager()->GetReport();
  }

  void MalwareRedirectCancelAndProceed(const std::string& open_function) {
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
    LOG(INFO) << __FUNCTION__;
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return false;
    // Wait until all <script> tags have executed, including jstemplate.
    // TODO(joaodasilva): it would be nice to avoid the busy loop, though in
    // practice it spins at most once or twice.
    std::string ready_state;
    do {
      scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
          rvh->GetMainFrame(), "document.readyState");
      if (!value.get() || !value->GetAsString(&ready_state))
        return false;
    } while (ready_state != "complete");
    LOG(INFO) << "done waiting";
    return true;
  }

  Visibility GetVisibility(const std::string& node_id) {
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return VISIBILITY_ERROR;
    scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
        rvh->GetMainFrame(),
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
    LOG(INFO) << "Click " << node_id;
    content::RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return false;
    // We don't use ExecuteScriptAndGetValue for this one, since clicking
    // the button/link may navigate away before the injected javascript can
    // reply, hanging the test.
    rvh->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16(
            "document.getElementById('" + node_id + "').click();\n"));
    return true;
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    // We wait for interstitial_detached rather than nav_entry_committed, as
    // going back from a main-frame malware interstitial page will not cause a
    // nav entry committed event.
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    InterstitialObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::Closure(),
        loop_runner->QuitClosure());
    if (!Click(node_id))
      return false;
    loop_runner->Run();
    return true;
  }

 protected:
  TestMalwareDetailsFactory details_factory_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageBrowserTest);
};

INSTANTIATE_TEST_CASE_P(SafeBrowsingInterstitialVersions,
                        SafeBrowsingBlockingPageBrowserTest,
                        testing::Values(2, 3));

// TODO(linux_aura) http://crbug.com/163931
// TODO(win_aura) http://crbug.com/154081
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_MalwareRedirectInIFrameCanceled DISABLED_MalwareRedirectInIFrameCanceled
#else
#define MAYBE_MalwareRedirectInIFrameCanceled MalwareRedirectInIFrameCanceled
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_MalwareRedirectInIFrameCanceled) {
  // 1. Test the case that redirect is a subresource.
  MalwareRedirectCancelAndProceed("openWinIFrame");
  // If the redirect was from subresource but canceled, "proceed" will continue
  // with the rest of resources.
  AssertNoInterstitial(true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MalwareRedirectCanceled) {
  // 2. Test the case that redirect is the only resource.
  MalwareRedirectCancelAndProceed("openWin");
  // Clicking proceed won't do anything if the main request is cancelled
  // already.  See crbug.com/76460.
  EXPECT_TRUE(YesInterstitial());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MalwareDontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  if (GetParam() == 2) {
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
  } else {
    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("details"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  }

  AssertNoInterstitial(false);   // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, MalwareProceed) {
  GURL url = SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  if (GetParam() == 2)
    EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  else
    EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MalwareLearnMoreV2) {
  if (GetParam() == 3) return;  // Don't have this link in V3.
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_TRUE(ClickAndWaitForDetach("learn-more-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the help page.
  EXPECT_EQ(
      "/transparencyreport/safebrowsing/",
       browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MalwareIframeDontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  SetupMalwareIframeWarningAndNavigate();

  if (GetParam() == 2) {
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
  } else {
    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("details"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  }

  AssertNoInterstitial(false);  // Assert the interstitial is gone

  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
    MalwareIframeProceed) {
  GURL url = SetupMalwareIframeWarningAndNavigate();

  if (GetParam() == 2)
    EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  else
    EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// http://crbug.com/273302
#if defined(OS_WIN)
// Temporarily re-enabled to get some logs.
#define MAYBE_MalwareIframeReportDetails MalwareIframeReportDetails
#else
#define MAYBE_MalwareIframeReportDetails MalwareIframeReportDetails
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_MalwareIframeReportDetails) {
  // TODO(felt): Enable for V3 when the checkbox is added.
  if (GetParam() == 3) return;

  scoped_refptr<content::MessageLoopRunner> malware_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(malware_report_sent_runner->QuitClosure());

  GURL url = SetupMalwareIframeWarningAndNavigate();

  LOG(INFO) << "1";

  // If the DOM details from renderer did not already return, wait for them.
  details_factory_.get_details()->WaitForDOM();
  LOG(INFO) << "2";

  EXPECT_TRUE(Click("check-report"));
  LOG(INFO) << "3";

  EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  LOG(INFO) << "4";
  AssertNoInterstitial(true);  // Assert the interstitial is gone
  LOG(INFO) << "5";

  ASSERT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled));
  LOG(INFO) << "6";

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  LOG(INFO) << "7";

  malware_report_sent_runner->Run();
  std::string serialized = GetReportSent();
  safe_browsing::ClientMalwareReportRequest report;
  ASSERT_TRUE(report.ParseFromString(serialized));
  // Verify the report is complete.
  EXPECT_TRUE(report.complete());
  LOG(INFO) << "8";
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the malware site.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ProceedDisabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  if (GetParam() == 3) return;

  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_MALWARE);

  if (GetParam() == 2) {
    EXPECT_EQ(VISIBLE, GetVisibility("check-report"));
    EXPECT_EQ(HIDDEN, GetVisibility("show-diagnostic-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-span"));
    EXPECT_TRUE(Click("see-more-link"));
    EXPECT_EQ(VISIBLE, GetVisibility("show-diagnostic-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-span"));
    EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  } else {
    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
    SendCommand("proceed");
  }

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
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  // TODO(felt): Enable for V3 when the checkbox is added.
  if (GetParam() == 3) return;

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled, true);

  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, net::SpawnedTestServer::kLocalhost,
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
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
    PhishingDontProceed) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  if (GetParam() == 2) {
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
  } else {
    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("details"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  }

  AssertNoInterstitial(false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // We are back to "about:blank".
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// http://crbug.com/247763
#if defined(OS_WIN)
// Temporarily re-enabled to get some logs.
#define MAYBE_PhishingProceed PhishingProceed
#else
#define MAYBE_PhishingProceed PhishingProceed
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
    MAYBE_PhishingProceed) {
  GURL url = SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);
  LOG(INFO) << "1";

  if (GetParam() == 2)
    EXPECT_TRUE(ClickAndWaitForDetach("proceed"));
  else
    EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  LOG(INFO) << "2";
  AssertNoInterstitial(true);  // Assert the interstitial is gone
  LOG(INFO) << "3";
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  LOG(INFO) << "4";
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
    PhishingReportErrorV2) {
  if (GetParam() == 3) return;  // Not supported in V3.
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_TRUE(ClickAndWaitForDetach("report-error-link"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  // We are in the error reporting page.
  EXPECT_EQ(
      "/safebrowsing/report_error/",
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
}

// See crbug.com/248447
#if defined(OS_WIN)
// Temporarily re-enabled to get some logs.
#define MAYBE_PhishingLearnMore PhishingLearnMore
#else
#define MAYBE_PhishingLearnMore PhishingLearnMore
#endif

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
    MAYBE_PhishingLearnMore) {
  SetupWarningAndNavigate(SB_THREAT_TYPE_URL_PHISHING);
  LOG(INFO) << "1";

  if (GetParam() == 2)
    EXPECT_TRUE(ClickAndWaitForDetach("learn-more-link"));
  else
    EXPECT_TRUE(ClickAndWaitForDetach("help-link"));
  LOG(INFO) << "2";
  AssertNoInterstitial(false);  // Assert the interstitial is gone

  LOG(INFO) << "3";
  // We are in the help page.
  EXPECT_EQ(
      "/transparencyreport/safebrowsing/",
       browser()->tab_strip_model()->GetActiveWebContents()->GetURL().path());
  LOG(INFO) << "4";
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/cert_status_flags.h"
#include "net/test/test_server.h"

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

class SSLUITest : public InProcessBrowserTest {
  typedef net::TestServer::HTTPSOptions HTTPSOptions;

 public:
  SSLUITest()
      : https_server_(
            HTTPSOptions(HTTPSOptions::CERT_OK), FilePath(kDocRoot)),
        https_server_expired_(
            HTTPSOptions(HTTPSOptions::CERT_EXPIRED), FilePath(kDocRoot)),
        https_server_mismatched_(
            HTTPSOptions(HTTPSOptions::CERT_MISMATCHED_NAME),
            FilePath(kDocRoot)) {
    EnableDOMAutomation();
  }

  void CheckAuthenticatedState(TabContents* tab,
                               bool displayed_insecure_content) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(NORMAL_PAGE, entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, entry->ssl().security_style());
    EXPECT_EQ(0, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_EQ(displayed_insecure_content,
              entry->ssl().displayed_insecure_content());
    EXPECT_FALSE(entry->ssl().ran_insecure_content());
  }

  void CheckUnauthenticatedState(TabContents* tab) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(NORMAL_PAGE, entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, entry->ssl().security_style());
    EXPECT_EQ(0, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(entry->ssl().displayed_insecure_content());
    EXPECT_FALSE(entry->ssl().ran_insecure_content());
  }

  void CheckAuthenticationBrokenState(TabContents* tab,
                                      int error,
                                      bool ran_insecure_content,
                                      bool interstitial) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(interstitial ? INTERSTITIAL_PAGE : NORMAL_PAGE,
              entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
              entry->ssl().security_style());
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION doesn't lower the security style
    // to SECURITY_STYLE_AUTHENTICATION_BROKEN.
    ASSERT_NE(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, error);
    EXPECT_EQ(error, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(entry->ssl().displayed_insecure_content());
    EXPECT_EQ(ran_insecure_content, entry->ssl().ran_insecure_content());
  }

  void CheckWorkerLoadResult(TabContents* tab, bool expectLoaded) {
    // Workers are async and we don't have notifications for them passing
    // messages since they do it between renderer and worker processes.
    // So have a polling loop, check every 200ms, timeout at 30s.
    const int timeout_ms = 200;
    base::Time timeToQuit = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(30000);

    while (base::Time::Now() < timeToQuit) {
      bool workerFinished = false;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
          tab->render_view_host(), std::wstring(),
          L"window.domAutomationController.send(IsWorkerFinished());",
          &workerFinished));

      if (workerFinished)
        break;

      // Wait a bit.
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE, new MessageLoop::QuitTask, timeout_ms);
      ui_test_utils::RunMessageLoop();
    }

    bool actuallyLoadedContent = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        tab->render_view_host(), std::wstring(),
        L"window.domAutomationController.send(IsContentLoaded());",
        &actuallyLoadedContent));
    EXPECT_EQ(expectLoaded, actuallyLoadedContent);
  }

  void ProceedThroughInterstitial(TabContents* tab) {
    InterstitialPage* interstitial_page = tab->interstitial_page();
    ASSERT_TRUE(interstitial_page);
    interstitial_page->Proceed();
    // Wait for the navigation to be done.
    ui_test_utils::WaitForNavigation(&(tab->controller()));
  }

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::TestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }

  static bool GetTopFramePath(const net::TestServer& http_server,
                              const net::TestServer& good_https_server,
                              const net::TestServer& bad_https_server,
                              std::string* top_frame_path) {
    // The "frame_left.html" page contained in the top_frame.html page contains
    // <a href>'s to three different servers. This sets up all of the
    // replacement text to work with test servers which listen on ephemeral
    // ports.
    GURL http_url = http_server.GetURL("files/ssl/google.html");
    GURL good_https_url = good_https_server.GetURL("files/ssl/google.html");
    GURL bad_https_url = bad_https_server.GetURL(
        "files/ssl/bad_iframe.html");

    std::vector<net::TestServer::StringPair> replacement_text_frame_left;
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_HTTP_PAGE", http_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
    std::string frame_left_path;
    if (!net::TestServer::GetFilePathWithReplacements(
            "frame_left.html",
            replacement_text_frame_left,
            &frame_left_path))
      return false;

    // Substitute the generated frame_left URL into the top_frame page.
    std::vector<net::TestServer::StringPair> replacement_text_top_frame;
    replacement_text_top_frame.push_back(
        make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
    return net::TestServer::GetFilePathWithReplacements(
        "files/ssl/top_frame.html",
        replacement_text_top_frame,
        top_frame_path);
  }

  static bool GetPageWithUnsafeWorkerPath(
      const net::TestServer& expired_https_server,
      std::string* page_with_unsafe_worker_path) {
    // Get the "imported.js" URL from the expired https server and
    // substitute it into the unsafe_worker.js file.
    GURL imported_js_url = expired_https_server.GetURL("files/ssl/imported.js");
    std::vector<net::TestServer::StringPair> replacement_text_for_unsafe_worker;
    replacement_text_for_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_IMPORTED_JS_URL", imported_js_url.spec()));
    std::string unsafe_worker_path;
    if (!net::TestServer::GetFilePathWithReplacements(
        "unsafe_worker.js",
        replacement_text_for_unsafe_worker,
        &unsafe_worker_path))
      return false;

    // Now, substitute this into the page with unsafe worker.
    std::vector<net::TestServer::StringPair>
        replacement_text_for_page_with_unsafe_worker;
    replacement_text_for_page_with_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_UNSAFE_WORKER_PATH", unsafe_worker_path));
    return net::TestServer::GetFilePathWithReplacements(
        "files/ssl/page_with_unsafe_worker.html",
        replacement_text_for_page_with_unsafe_worker,
        page_with_unsafe_worker_path);
  }

  net::TestServer https_server_;
  net::TestServer https_server_expired_;
  net::TestServer https_server_mismatched_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL("files/ssl/google.html"));

  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page over http which includes broken https resources (status should
// be OK).
// TODO(jcampan): test that bad HTTPS content is blocked (otherwise we'll give
//                the secure cookies away!).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_unsafe_contents.html",
      https_server_expired_.host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL(replacement_path));

  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("files/ssl/google.html"));

  CheckAuthenticatedState(browser()->GetSelectedTabContents(), false);
}

// Visits a page with https error and proceed:
// Disabled, http://crbug.com/68448.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No interstitial showing
}

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
#if defined(OS_WIN)
// Disabled, flakily exceeds test timeout, http://crbug.com/43575.
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
    DISABLED_TestHTTPSExpiredCertAndDontProceed
#else
// Marked as flaky, see bug 40932.
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
    FLAKY_TestHTTPSExpiredCertAndDontProceed
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSExpiredCertAndDontProceed) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an OK page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("files/ssl/google.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry);

  GURL cross_site_url =
      https_server_expired_.GetURL("files/ssl/google.html");
  // Change the host name from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation so we can test http://crbug.com/5800 is gone.
  ASSERT_EQ("127.0.0.1", cross_site_url.host());
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  cross_site_url = cross_site_url.ReplaceComponents(replacements);

  // Now go to a bad HTTPS page.
  ui_test_utils::NavigateToURL(browser(), cross_site_url);

  // An interstitial should be showing.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false, true);

  // Simulate user clicking "Take me back".
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->DontProceed();

  // We should be back to the original good page.
  CheckAuthenticatedState(tab, false);

  // Try to navigate to a new page. (to make sure bug 5800 is fixed).
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL("files/ssl/google.html"));
  CheckUnauthenticatedState(tab);
}

// Visits a page with https error and then goes back using Browser::GoBack.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoBackViaButton) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  TabContents* tab = browser()->GetSelectedTabContents();
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing

  // Simulate user clicking on back button (crbug.com/39248).
  browser()->GoBack(CURRENT_TAB);

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->GetSelectedTabContents()->interstitial_page());
  CheckUnauthenticatedState(tab);
}

// Visits a page with https error and then goes back using GoToOffset.
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestHTTPSExpiredCertAndGoBackViaMenu) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  TabContents* tab = browser()->GetSelectedTabContents();
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing

  // Simulate user clicking and holding on back button (crbug.com/37215).
  tab->controller().GoToOffset(-1);

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->GetSelectedTabContents()->interstitial_page());
  CheckUnauthenticatedState(tab);
}

// Visits a page with https error and then goes forward using GoToOffset.
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestHTTPSExpiredCertAndGoForward) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to two HTTP pages.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  TabContents* tab = browser()->GetSelectedTabContents();
  NavigationEntry* entry1 = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry1);
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/blank_page.html"));
  NavigationEntry* entry2 = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry2);

  // Now go back so that a page is in the forward history.
  tab->controller().GoBack();
  ui_test_utils::WaitForNavigation(&(tab->controller()));
  ASSERT_TRUE(tab->controller().CanGoForward());
  NavigationEntry* entry3 = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry1 == entry3);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing

  // Simulate user clicking and holding on forward button.
  tab->controller().GoToOffset(1);
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  // We should be showing the second good page.
  EXPECT_FALSE(browser()->GetSelectedTabContents()->interstitial_page());
  CheckUnauthenticatedState(tab);
  EXPECT_FALSE(tab->controller().CanGoForward());
  NavigationEntry* entry4 = tab->controller().GetActiveEntry();
  EXPECT_TRUE(entry2 == entry4);
}

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url = https_server_expired_.GetURL("files/ssl/google.htm");
  TabContentsWrapper* tab2 =
      browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);
  ui_test_utils::WaitForLoadStop(&(tab2->controller()));

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(browser()->command_updater()->IsCommandEnabled(IDC_BACK));

  // We should have an interstitial page showing.
  ASSERT_TRUE(tab2->tab_contents()->interstitial_page());
}

//
// Insecure content
//

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->GetSelectedTabContents(), true);
}

// Visits a page that runs insecure content and tries to suppress the insecure
// content warnings by randomizing location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecuredContentRandomizeHash) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      "files/ssl/page_runs_insecure_content.html"));

  CheckAuthenticationBrokenState(browser()->GetSelectedTabContents(), 0, true,
                                 false);
}

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_unsafe_contents.html",
      https_server_expired_.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  TabContents* tab = browser()->GetSelectedTabContents();
  // When the bad content is filtered, the state is expected to be
  // authenticated.
  CheckAuthenticatedState(tab, false);

  // Because of cross-frame scripting restrictions, we cannot access the iframe
  // content.  So to know if the frame was loaded, we just check if a popup was
  // opened (the iframe content opens one).
  // Note: because of bug 1115868, no constrained window is opened right now.
  //       Once the bug is fixed, this will do the real check.
  EXPECT_EQ(0, static_cast<int>(tab->constrained_window_count()));

  int img_width;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_LT(img_width, 100);

  bool js_result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_FALSE(js_result);
}

// Visits a page with insecure content loaded by JS (after the initial page
// load).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentLoadedFromJS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_dynamic_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      replacement_path));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticatedState(tab, false);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(), L"loadBadImage();", &js_result));
  EXPECT_TRUE(js_result);

  // We should now have insecure content.
  CheckAuthenticatedState(tab, true);
}

// Visits two pages from the same origin: one that displays insecure content and
// one that doesn't.  The test checks that we do not propagate the insecure
// content state from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentTwoTabs) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_.GetURL("files/ssl/blank_page.html"));

  TabContentsWrapper* tab1 = browser()->GetSelectedTabContentsWrapper();

  // This tab should be fine.
  CheckAuthenticatedState(tab1->tab_contents(), false);

  // Create a new tab.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  GURL url = https_server_.GetURL(replacement_path);
  browser::NavigateParams params(browser(), url, PageTransition::TYPED);
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  browser::Navigate(&params);
  TabContentsWrapper* tab2 = params.target_contents;
  ui_test_utils::WaitForNavigation(&(tab2->controller()));

  // The new tab has insecure content.
  CheckAuthenticatedState(tab2->tab_contents(), true);

  // The original tab should not be contaminated.
  CheckAuthenticatedState(tab1->tab_contents(), false);
}

// Visits two pages from the same origin: one that runs insecure content and one
// that doesn't.  The test checks that we propagate the insecure content state
// from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecureContentTwoTabs) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_.GetURL("files/ssl/blank_page.html"));

  TabContentsWrapper* tab1 = browser()->GetSelectedTabContentsWrapper();

  // This tab should be fine.
  CheckAuthenticatedState(tab1->tab_contents(), false);

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_runs_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  // Create a new tab.
  GURL url = https_server_.GetURL(replacement_path);
  browser::NavigateParams params(browser(), url, PageTransition::TYPED);
  params.disposition = NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  browser::Navigate(&params);
  TabContentsWrapper* tab2 = params.target_contents;
  ui_test_utils::WaitForNavigation(&(tab2->controller()));

  // The new tab has insecure content.
  CheckAuthenticationBrokenState(tab2->tab_contents(), 0, true, false);

  // Which means the origin for the first tab has also been contaminated with
  // insecure content.
  CheckAuthenticationBrokenState(tab1->tab_contents(), 0, true, false);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysCachedInsecureContent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  // Load original page over HTTP.
  const GURL url_http = test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckUnauthenticatedState(tab);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckAuthenticatedState(tab, true);
}

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsCachedInsecureContent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_runs_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  // Load original page over HTTP.
  const GURL url_http = test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckUnauthenticatedState(tab);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckAuthenticationBrokenState(tab, 0, true, false);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// Disabled, see http://crbug.com/68448 and http://crbug.com/49377.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ui_test_utils::NavigateToURL(browser(),
      https_server_mismatched_.GetURL("files/ssl/google.html"));

  // We get an interstitial page as a result.
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false, true);  // Interstitial showing.
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false, false);  // No interstitial showing.

  // Now we try again with the right host name this time.
  GURL url(https_server_.GetURL("files/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Security state should be OK.
  CheckAuthenticatedState(tab, false);

  // Now try again the broken one to make sure it is still broken.
  ui_test_utils::NavigateToURL(browser(),
      https_server_mismatched_.GetURL("files/ssl/google.html"));

  // Since we OKed the interstitial last time, we get right to the page.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false, false);  // No interstitial showing.
}

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/page_with_refs.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing.

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No interstitial showing.

  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/page_with_refs.html#jp"));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No interstitial showing.
}

// Tests that closing a page that has a unsafe pop-up does not crash the
// browser (bug #1966).
// TODO(jcampan): http://crbug.com/2136 disabled because the popup is not
//                opened as it is not initiated by a user gesture.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCloseTabWithUnsafePopup) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_unsafe_popup.html",
      https_server_expired_.host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  TabContents* tab1 = browser()->GetSelectedTabContents();
  // It is probably overkill to add a notification for a popup-opening, let's
  // just poll.
  for (int i = 0; i < 10; i++) {
    if (static_cast<int>(tab1->constrained_window_count()) > 0)
      break;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(), 1000);
    ui_test_utils::RunMessageLoop();
  }
  ASSERT_EQ(1, static_cast<int>(tab1->constrained_window_count()));

  // Let's add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = test_server()->GetURL("files/ssl/google.html");
  TabContentsWrapper* tab2 =
      browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);
  ui_test_utils::WaitForNavigation(&(tab2->controller()));

  // Close the first tab.
  browser()->CloseTabContents(tab1);
}

// Visit a page over bad https that is a redirect to a page with good https.
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestRedirectBadToGoodHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_expired_.GetURL("server-redirect?");
  GURL url2 = https_server_.GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  TabContents* tab = browser()->GetSelectedTabContents();

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing.

  ProceedThroughInterstitial(tab);

  // We have been redirected to the good page.
  CheckAuthenticatedState(tab, false);
}

// Visit a page over good https that is a redirect to a page with bad https.
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("server-redirect?");
  GURL url2 = https_server_expired_.GetURL("files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing.

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No interstitial showing.
}

// Visit a page over http that is a redirect to a page with good HTTPS.
// Disabled, http://crbug.com/70216.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestRedirectHTTPToGoodHTTPS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  TabContents* tab = browser()->GetSelectedTabContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = test_server()->GetURL("server-redirect?");
  GURL good_https_url =
      https_server_.GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + good_https_url.spec()));
  CheckAuthenticatedState(tab, false);
}

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  TabContents* tab = browser()->GetSelectedTabContents();

  GURL http_url = test_server()->GetURL("server-redirect?");
  GURL bad_https_url =
      https_server_expired_.GetURL("files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing.

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No interstitial showing.
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestRedirectHTTPSToHTTP) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("server-redirect?");
  GURL http_url = test_server()->GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(https_url.spec() + http_url.spec()));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17"));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());

  // Same thing over HTTPS.
  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17"));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect insecure content), then back
// Disabled, http://crbug.com/18626.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestGoodFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(top_frame_path));

  CheckAuthenticatedState(tab, false);

  bool success = false;
  // Now navigate inside the frame.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be fine.
  CheckAuthenticatedState(tab, false);

  // Now let's hit a bad page.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // The security style should still be secure.
  CheckAuthenticatedState(tab, false);

  // And the frame should be blocked.
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_evil_js(L"window.domAutomationController.send("
                          L"document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), content_frame_xpath, is_evil_js,
      &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should still be OK.
  tab->controller().GoBack();
  ui_test_utils::WaitForNavigation(&tab->controller());
  CheckAuthenticatedState(tab, false);

  // Navigate to a page served over HTTP.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('HTTPLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // Our state should be insecure.
  CheckAuthenticatedState(tab, true);

  // Go back, our state should be unchanged.
  tab->controller().GoBack();
  ui_test_utils::WaitForNavigation(&tab->controller());
  CheckAuthenticatedState(tab, true);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestBadFrameNavigation) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(top_frame_path));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing

  ProceedThroughInterstitial(tab);

  // Navigate to a good frame.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be authentication broken.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX)
// Disabled, flakily exceeds test timeout, http://crbug.com/43437.
#define MAYBE_TestUnauthenticatedFrameNavigation \
      DISABLED_TestUnauthenticatedFrameNavigation
#else
// Marked as flaky, see bug 40932.
#define MAYBE_TestUnauthenticatedFrameNavigation \
      FLAKY_TestUnauthenticatedFrameNavigation
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestUnauthenticatedFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(top_frame_path));
  CheckUnauthenticatedState(tab);

  // Now navigate inside the frame to a secure HTTPS frame.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be unauthenticated.
  CheckUnauthenticatedState(tab);

  // Now navigate to a bad HTTPS frame.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), std::wstring(),
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // State should not have changed.
  CheckUnauthenticatedState(tab);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_evil_js(L"window.domAutomationController.send("
                          L"document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), content_frame_xpath, is_evil_js,
      &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}

// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestUnsafeContentsInWorkerFiltered) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  std::string page_with_unsafe_worker_path;
  ASSERT_TRUE(GetPageWithUnsafeWorkerPath(https_server_expired_,
                                          &page_with_unsafe_worker_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      page_with_unsafe_worker_path));
  TabContents* tab = browser()->GetSelectedTabContents();
  // Expect Worker not to load insecure content.
  CheckWorkerLoadResult(tab, false);
  // The bad content is filtered, expect the state to be authenticated.
  CheckAuthenticatedState(tab, false);
}

// Marked as flaky, see bug 40932.
IN_PROC_BROWSER_TEST_F(SSLUITest, FLAKY_TestUnsafeContentsInWorker) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to an unsafe site. Proceed with interstitial page to indicate
  // the user approves the bad certificate.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/blank_page.html"));
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 true);  // Interstitial showing
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false,
                                 false);  // No Interstitial

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  std::string page_with_unsafe_worker_path;
  ASSERT_TRUE(GetPageWithUnsafeWorkerPath(https_server_expired_,
                                          &page_with_unsafe_worker_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      page_with_unsafe_worker_path));
  CheckWorkerLoadResult(tab, true);  // Worker loads insecure content
  CheckAuthenticationBrokenState(tab, 0, true, false);
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.

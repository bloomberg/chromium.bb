// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/security_style.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_status_flags.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

#if defined(USE_NSS)
#include "chrome/browser/net/nss_context.h"
#include "net/base/crypto_module.h"
#include "net/cert/nss_cert_database.h"
#endif  // defined(USE_NSS)

using base::ASCIIToUTF16;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

namespace {

class ProvisionalLoadWaiter : public content::WebContentsObserver {
 public:
  explicit ProvisionalLoadWaiter(WebContents* tab)
    : WebContentsObserver(tab), waiting_(false), seen_(false) {}

  void Wait() {
    if (seen_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }

  virtual void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) OVERRIDE {
    seen_ = true;
    if (waiting_)
      base::MessageLoopForUI::current()->Quit();
  }

 private:
  bool waiting_;
  bool seen_;
};

namespace AuthState {

enum AuthStateFlags {
  NONE = 0,
  DISPLAYED_INSECURE_CONTENT = 1 << 0,
  RAN_INSECURE_CONTENT = 1 << 1,
  SHOWING_INTERSTITIAL = 1 << 2,
  SHOWING_ERROR = 1 << 3
};

void Check(const NavigationEntry& entry, int expected_authentication_state) {
  if (expected_authentication_state == AuthState::SHOWING_ERROR) {
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry.GetPageType());
  } else {
    EXPECT_EQ(
        !!(expected_authentication_state & AuthState::SHOWING_INTERSTITIAL)
            ? content::PAGE_TYPE_INTERSTITIAL
            : content::PAGE_TYPE_NORMAL,
        entry.GetPageType());
  }

  bool displayed_insecure_content =
      !!(entry.GetSSL().content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT);
  EXPECT_EQ(
      !!(expected_authentication_state & AuthState::DISPLAYED_INSECURE_CONTENT),
      displayed_insecure_content);

  bool ran_insecure_content =
      !!(entry.GetSSL().content_status & SSLStatus::RAN_INSECURE_CONTENT);
  EXPECT_EQ(!!(expected_authentication_state & AuthState::RAN_INSECURE_CONTENT),
            ran_insecure_content);
}

}  // namespace AuthState

namespace SecurityStyle {

void Check(const NavigationEntry& entry,
           content::SecurityStyle expected_security_style) {
  EXPECT_EQ(expected_security_style, entry.GetSSL().security_style);
}

}  // namespace SecurityStyle

namespace CertError {

enum CertErrorFlags {
  NONE = 0
};

void Check(const NavigationEntry& entry, net::CertStatus error) {
  if (error) {
    EXPECT_EQ(error, entry.GetSSL().cert_status & error);
    net::CertStatus extra_cert_errors =
        error ^ (entry.GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
    if (extra_cert_errors)
      LOG(WARNING) << "Got unexpected cert error: " << extra_cert_errors;
  } else {
    EXPECT_EQ(0U, entry.GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
  }
}

}  // namespace CertError

void CheckSecurityState(WebContents* tab,
                        net::CertStatus error,
                        content::SecurityStyle expected_security_style,
                        int expected_authentication_state) {
  ASSERT_FALSE(tab->IsCrashed());
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  CertError::Check(*entry, error);
  SecurityStyle::Check(*entry, expected_security_style);
  AuthState::Check(*entry, expected_authentication_state);
}

}  // namespace

class SSLUITest : public InProcessBrowserTest {
 public:
  SSLUITest()
      : https_server_(net::SpawnedTestServer::TYPE_HTTPS,
                      SSLOptions(SSLOptions::CERT_OK),
                      base::FilePath(kDocRoot)),
        https_server_expired_(net::SpawnedTestServer::TYPE_HTTPS,
                              SSLOptions(SSLOptions::CERT_EXPIRED),
                              base::FilePath(kDocRoot)),
        https_server_mismatched_(net::SpawnedTestServer::TYPE_HTTPS,
                                 SSLOptions(SSLOptions::CERT_MISMATCHED_NAME),
                                 base::FilePath(kDocRoot)),
        wss_server_expired_(net::SpawnedTestServer::TYPE_WSS,
                            SSLOptions(SSLOptions::CERT_EXPIRED),
                            net::GetWebSocketTestDataDirectory()) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    // Use process-per-site so that navigating to a same-site page in a
    // new tab will use the same process.
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

  void CheckAuthenticatedState(WebContents* tab,
                               int expected_authentication_state) {
    CheckSecurityState(tab,
                       CertError::NONE,
                       content::SECURITY_STYLE_AUTHENTICATED,
                       expected_authentication_state);
  }

  void CheckUnauthenticatedState(WebContents* tab,
                                 int expected_authentication_state) {
    CheckSecurityState(tab,
                       CertError::NONE,
                       content::SECURITY_STYLE_UNAUTHENTICATED,
                       expected_authentication_state);
  }

  void CheckAuthenticationBrokenState(WebContents* tab,
                                      net::CertStatus error,
                                      int expected_authentication_state) {
    CheckSecurityState(tab,
                       error,
                       content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
                       expected_authentication_state);
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION doesn't lower the security style
    // to SECURITY_STYLE_AUTHENTICATION_BROKEN.
    ASSERT_NE(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, error);
  }

  void CheckWorkerLoadResult(WebContents* tab, bool expected_load) {
    // Workers are async and we don't have notifications for them passing
    // messages since they do it between renderer and worker processes.
    // So have a polling loop, check every 200ms, timeout at 30s.
    const int kTimeoutMS = 200;
    base::Time time_to_quit = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(30000);

    while (base::Time::Now() < time_to_quit) {
      bool worker_finished = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          tab,
          "window.domAutomationController.send(IsWorkerFinished());",
          &worker_finished));

      if (worker_finished)
        break;

      // Wait a bit.
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::MessageLoop::QuitClosure(),
          base::TimeDelta::FromMilliseconds(kTimeoutMS));
      content::RunMessageLoop();
    }

    bool actually_loaded_content = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(IsContentLoaded());",
        &actually_loaded_content));
    EXPECT_EQ(expected_load, actually_loaded_content);
  }

  void ProceedThroughInterstitial(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    interstitial_page->Proceed();
    observer.Wait();
  }

  bool IsShowingWebContentsModalDialog() const {
    return WebContentsModalDialogManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())->
            IsDialogActive();
  }

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::SpawnedTestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::SpawnedTestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }

  static bool GetTopFramePath(const net::SpawnedTestServer& http_server,
                              const net::SpawnedTestServer& good_https_server,
                              const net::SpawnedTestServer& bad_https_server,
                              std::string* top_frame_path) {
    // The "frame_left.html" page contained in the top_frame.html page contains
    // <a href>'s to three different servers. This sets up all of the
    // replacement text to work with test servers which listen on ephemeral
    // ports.
    GURL http_url = http_server.GetURL("files/ssl/google.html");
    GURL good_https_url = good_https_server.GetURL("files/ssl/google.html");
    GURL bad_https_url = bad_https_server.GetURL(
        "files/ssl/bad_iframe.html");

    std::vector<net::SpawnedTestServer::StringPair> replacement_text_frame_left;
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_HTTP_PAGE", http_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
    std::string frame_left_path;
    if (!net::SpawnedTestServer::GetFilePathWithReplacements(
            "frame_left.html",
            replacement_text_frame_left,
            &frame_left_path))
      return false;

    // Substitute the generated frame_left URL into the top_frame page.
    std::vector<net::SpawnedTestServer::StringPair> replacement_text_top_frame;
    replacement_text_top_frame.push_back(
        make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
    return net::SpawnedTestServer::GetFilePathWithReplacements(
        "files/ssl/top_frame.html",
        replacement_text_top_frame,
        top_frame_path);
  }

  static bool GetPageWithUnsafeWorkerPath(
      const net::SpawnedTestServer& expired_https_server,
      std::string* page_with_unsafe_worker_path) {
    // Get the "imported.js" URL from the expired https server and
    // substitute it into the unsafe_worker.js file.
    GURL imported_js_url = expired_https_server.GetURL("files/ssl/imported.js");
    std::vector<net::SpawnedTestServer::StringPair>
        replacement_text_for_unsafe_worker;
    replacement_text_for_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_IMPORTED_JS_URL", imported_js_url.spec()));
    std::string unsafe_worker_path;
    if (!net::SpawnedTestServer::GetFilePathWithReplacements(
        "unsafe_worker.js",
        replacement_text_for_unsafe_worker,
        &unsafe_worker_path))
      return false;

    // Now, substitute this into the page with unsafe worker.
    std::vector<net::SpawnedTestServer::StringPair>
        replacement_text_for_page_with_unsafe_worker;
    replacement_text_for_page_with_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_UNSAFE_WORKER_PATH", unsafe_worker_path));
    return net::SpawnedTestServer::GetFilePathWithReplacements(
        "files/ssl/page_with_unsafe_worker.html",
        replacement_text_for_page_with_unsafe_worker,
        page_with_unsafe_worker_path);
  }

  net::SpawnedTestServer https_server_;
  net::SpawnedTestServer https_server_expired_;
  net::SpawnedTestServer https_server_mismatched_;
  net::SpawnedTestServer wss_server_expired_;

 private:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

class SSLUITestBlock : public SSLUITest {
 public:
  SSLUITestBlock() : SSLUITest() {}

  // Browser will neither run nor display insecure content.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kNoDisplayingInsecureContent);
  }
};

class SSLUITestIgnoreCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrors() : SSLUITest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Browser will ignore certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL("files/ssl/google.html"));

  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
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

  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithInsecureContent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_DATE_INVALID,
                                 AuthState::DISPLAYED_INSECURE_CONTENT);
}

// http://crbug.com/91745
#if defined(OS_CHROMEOS)
#define MAYBE_TestOKHTTPS DISABLED_TestOKHTTPS
#else
#define MAYBE_TestOKHTTPS TestOKHTTPS
#endif

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("files/ssl/google.html"));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Visits a page with https error and proceed:
#if defined(OS_LINUX)
// flaky http://crbug.com/396462
#define MAYBE_TestHTTPSExpiredCertAndProceed \
    DISABLED_TestHTTPSExpiredCertAndProceed
#else
#define MAYBE_TestHTTPSExpiredCertAndProceed TestHTTPSExpiredCertAndProceed
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

#ifndef NEDBUG
// Flaky on Windows debug (http://crbug.com/280537).
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
        DISABLED_TestHTTPSExpiredCertAndDontProceed
#else
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
        TestHTTPSExpiredCertAndDontProceed
#endif

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSExpiredCertAndDontProceed) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an OK page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("files/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
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
  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking "Take me back".
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->DontProceed();

  // We should be back to the original good page.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Try to navigate to a new page. (to make sure bug 5800 is fixed).
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL("files/ssl/google.html"));
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using Browser::GoBack.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestHTTPSExpiredCertAndGoBackViaButton) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProvisionalLoadWaiter load_failed_observer(tab);

  // Simulate user clicking on back button (crbug.com/39248).
  chrome::GoBack(browser(), CURRENT_TAB);

  // Wait until we hear the load failure, and make sure we haven't swapped out
  // the previous page.  Prevents regression of http://crbug.com/82667.
  load_failed_observer.Wait();
  EXPECT_FALSE(content::RenderViewHostTester::IsRenderViewHostSwappedOut(
      tab->GetRenderViewHost()));

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using GoToOffset.
// Disabled because its flaky: http://crbug.com/40932, http://crbug.com/43575.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestHTTPSExpiredCertAndGoBackViaMenu) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on back button (crbug.com/37215).
  tab->GetController().GoToOffset(-1);

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes forward using GoToOffset.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoForward) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to two HTTP pages.
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry1 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry1);
  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/ssl/blank_page.html"));
  NavigationEntry* entry2 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry2);

  // Now go back so that a page is in the forward history.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }
  ASSERT_TRUE(tab->GetController().CanGoForward());
  NavigationEntry* entry3 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry1 == entry3);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on forward button.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoToOffset(1);
    observer.Wait();
  }

  // We should be showing the second good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
  EXPECT_FALSE(tab->GetController().CanGoForward());
  NavigationEntry* entry4 = tab->GetController().GetActiveEntry();
  EXPECT_TRUE(entry2 == entry4);
}

// Visit a HTTP page which request WSS connection to a server providing invalid
// certificate. Close the page while WSS connection waits for SSLManager's
// response from UI thread.
// Disabled on Windows because it was flaking on XP Tests (1). crbug.com/165258
#if defined(OS_WIN)
#define MAYBE_TestWSSInvalidCertAndClose DISABLED_TestWSSInvalidCertAndClose
#else
#define MAYBE_TestWSSInvalidCertAndClose TestWSSInvalidCertAndClose
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestWSSInvalidCertAndClose) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Create GURLs to test pages.
  std::string master_url_path = base::StringPrintf("%s?%d",
      test_server()->GetURL("files/ssl/wss_close.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL master_url(master_url_path);
  std::string slave_url_path = base::StringPrintf("%s?%d",
      test_server()->GetURL("files/ssl/wss_close_slave.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL slave_url(slave_url_path);

  // Create tabs and visit pages which keep on creating wss connections.
  WebContents* tabs[16];
  for (int i = 0; i < 16; ++i) {
    tabs[i] = chrome::AddSelectedTabWithURL(browser(), slave_url,
                                            ui::PAGE_TRANSITION_LINK);
  }
  chrome::SelectNextTab(browser());

  // Visit a page which waits for one TLS handshake failure.
  // The title will be changed to 'PASS'.
  ui_test_utils::NavigateToURL(browser(), master_url);
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(LowerCaseEqualsASCII(result, "pass"));

  // Close tabs which contains the test page.
  for (int i = 0; i < 16; ++i)
    chrome::CloseWebContents(browser(), tabs[i], false);
  chrome::CloseWebContents(browser(), tab, false);
}

// Visit a HTTPS page and proceeds despite an invalid certificate. The page
// requests WSS connection to the same origin host to check if WSS connection
// share certificates policy with HTTPS correcly.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCertAndGoForward) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_expired_.GetURL(
          "connect_check.html").ReplaceComponents(replacements));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Proceed anyway.
  ProceedThroughInterstitial(tab);

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(LowerCaseEqualsASCII(result, "pass"));
}

#if defined(USE_NSS)
class SSLUITestWithClientCert : public SSLUITest {
  public:
   SSLUITestWithClientCert() : cert_db_(NULL) {}

   virtual void SetUpOnMainThread() OVERRIDE {
     SSLUITest::SetUpOnMainThread();

     base::RunLoop loop;
     GetNSSCertDatabaseForProfile(
         browser()->profile(),
         base::Bind(&SSLUITestWithClientCert::DidGetCertDatabase,
                    base::Unretained(this),
                    &loop));
     loop.Run();
   }

  protected:
   void DidGetCertDatabase(base::RunLoop* loop, net::NSSCertDatabase* cert_db) {
     cert_db_ = cert_db;
     loop->Quit();
   }

   net::NSSCertDatabase* cert_db_;
};

// SSL client certificate tests are only enabled when using NSS for private key
// storage, as only NSS can avoid modifying global machine state when testing.
// See http://crbug.com/51132

// Visit a HTTPS page which requires client cert authentication. The client
// cert will be selected automatically, then a test which uses WebSocket runs.
IN_PROC_BROWSER_TEST_F(SSLUITestWithClientCert, TestWSSClientCert) {
  // Import a client cert for test.
  scoped_refptr<net::CryptoModule> crypt_module = cert_db_->GetPublicModule();
  std::string pkcs12_data;
  base::FilePath cert_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_client_cert.p12"));
  EXPECT_TRUE(base::ReadFileToString(cert_path, &pkcs12_data));
  EXPECT_EQ(net::OK,
            cert_db_->ImportFromPKCS12(
                crypt_module.get(), pkcs12_data, base::string16(), true, NULL));

  // Start WebSocket test server with TLS and client cert authentication.
  net::SpawnedTestServer::SSLOptions options(
      net::SpawnedTestServer::SSLOptions::CERT_OK);
  options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_cacert.pem"));
  options.client_authorities.push_back(ca_path);
  net::SpawnedTestServer wss_server(net::SpawnedTestServer::TYPE_WSS,
                             options,
                             net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(wss_server.Start());
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  GURL url = wss_server.GetURL("connect_check.html").ReplaceComponents(
      replacements);

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("ISSUER.CN", "pywebsocket");
  profile->GetHostContentSettingsMap()->SetWebsiteSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::FromURL(url),
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
      std::string(),
      dict.release());

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURL(browser(), url);
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Test page runs a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(LowerCaseEqualsASCII(result, "pass"));
}
#endif  // defined(USE_NSS)

// Flaky on CrOS http://crbug.com/92292
#if defined(OS_CHROMEOS)
#define MAYBE_TestHTTPSErrorWithNoNavEntry \
    DISABLED_TestHTTPSErrorWithNoNavEntry
#else
#define MAYBE_TestHTTPSErrorWithNoNavEntry TestHTTPSErrorWithNoNavEntry
#endif  // defined(OS_CHROMEOS)

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url = https_server_expired_.GetURL("files/ssl/google.htm");
  WebContents* tab2 = chrome::AddSelectedTabWithURL(
      browser(), url, ui::PAGE_TRANSITION_TYPED);
  content::WaitForLoadStop(tab2);

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We should have an interstitial page showing.
  ASSERT_TRUE(tab2->GetInterstitialPage());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadHTTPSDownload) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL url_non_dangerous = test_server()->GetURL(std::string());
  GURL url_dangerous =
      https_server_expired_.GetURL("files/downloads/dangerous/dangerous.exe");
  base::ScopedTempDir downloads_directory_;

  // Need empty temp dir to avoid having Chrome ask us for a new filename
  // when we've downloaded dangerous.exe one hundred times.
  ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory,
      downloads_directory_.path());

  // Visit a non-dangerous page.
  ui_test_utils::NavigateToURL(browser(), url_non_dangerous);

  // Now, start a transition to dangerous download.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::NavigateParams navigate_params(browser(), url_dangerous,
                                           ui::PAGE_TRANSITION_TYPED);
    chrome::Navigate(&navigate_params);
    observer.Wait();
  }

  // To exit the browser cleanly (and this test) we need to complete the
  // download after completing this test.
  content::DownloadTestObserverTerminal dangerous_download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);

  // Proceed through the SSL interstitial. This doesn't use
  // |ProceedThroughInterstitial| since no page load will commit.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab != NULL);
  ASSERT_TRUE(tab->GetInterstitialPage() != NULL);
  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_DOWNLOAD_INITIATED,
        content::NotificationService::AllSources());
    tab->GetInterstitialPage()->Proceed();
    observer.Wait();
  }

  // There should still be an interstitial at this point. Press the
  // back button on the browser. Note that this doesn't wait for a
  // NAV_ENTRY_COMMITTED notification because going back with an
  // active interstitial simply hides the interstitial.
  ASSERT_TRUE(tab->GetInterstitialPage() != NULL);
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), CURRENT_TAB);

  dangerous_download_observer.WaitForFinished();
}

//
// Insecure content
//

#if defined(OS_WIN)
// http://crbug.com/152940 Flaky on win.
#define MAYBE_TestDisplaysInsecureContent DISABLED_TestDisplaysInsecureContent
#else
#define MAYBE_TestDisplaysInsecureContent TestDisplaysInsecureContent
#endif

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestDisplaysInsecureContent) {
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

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page that runs insecure content and tries to suppress the insecure
// content warnings by randomizing location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestRunsInsecuredContentRandomizeHash) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      "files/ssl/page_runs_insecure_content.html"));

  CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);
}

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_unsafe_contents.html",
      https_server_expired_.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // When the bad content is filtered, the state is expected to be
  // authenticated.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Because of cross-frame scripting restrictions, we cannot access the iframe
  // content.  So to know if the frame was loaded, we just check if a popup was
  // opened (the iframe content opens one).
  // Note: because of bug 1115868, no web contents modal dialog is opened right
  //       now.  Once the bug is fixed, this will do the real check.
  EXPECT_FALSE(IsShowingWebContentsModalDialog());

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab,
      "window.domAutomationController.send(ImageWidth());",
      &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_LT(img_width, 100);

  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(IsFooSet());",
      &js_result));
  EXPECT_FALSE(js_result);
}

// Visits a page with insecure content loaded by JS (after the initial page
// load).
#if defined(OS_LINUX)
// flaky http://crbug.com/396462
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
    DISABLED_TestDisplaysInsecureContentLoadedFromJS
#else
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
    TestDisplaysInsecureContentLoadedFromJS
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       MAYBE_TestDisplaysInsecureContentLoadedFromJS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_with_dynamic_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);

  // We should now have insecure content.
  CheckAuthenticatedState(tab, AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits two pages from the same origin: one that displays insecure content and
// one that doesn't.  The test checks that we do not propagate the insecure
// content state from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentTwoTabs) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_.GetURL("files/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  CheckAuthenticatedState(tab1, AuthState::NONE);

  // Create a new tab.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  GURL url = https_server_.GetURL(replacement_path);
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // The new tab has insecure content.
  CheckAuthenticatedState(tab2, AuthState::DISPLAYED_INSECURE_CONTENT);

  // The original tab should not be contaminated.
  CheckAuthenticatedState(tab1, AuthState::NONE);
}

// Visits two pages from the same origin: one that runs insecure content and one
// that doesn't.  The test checks that we propagate the insecure content state
// from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecureContentTwoTabs) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_.GetURL("files/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  CheckAuthenticatedState(tab1, AuthState::NONE);

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_runs_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  // Create a new tab in the same process.  Using a NEW_FOREGROUND_TAB
  // disposition won't usually stay in the same process, but this works
  // because we are using process-per-site in SetUpCommandLine.
  GURL url = https_server_.GetURL(replacement_path);
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // Both tabs should have the same process.
  EXPECT_EQ(tab1->GetRenderProcessHost(), tab2->GetRenderProcessHost());

  // The new tab has insecure content.
  CheckAuthenticationBrokenState(
      tab2,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);

  // Which means the origin for the first tab has also been contaminated with
  // insecure content.
  CheckAuthenticationBrokenState(
      tab1, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
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
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckAuthenticatedState(tab, AuthState::DISPLAYED_INSECURE_CONTENT);
}

// http://crbug.com/84729
#if defined(OS_CHROMEOS)
#define MAYBE_TestRunsCachedInsecureContent \
    DISABLED_TestRunsCachedInsecureContent
#else
#define MAYBE_TestRunsCachedInsecureContent TestRunsCachedInsecureContent
#endif  // defined(OS_CHROMEOS)

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRunsCachedInsecureContent) {
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
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckAuthenticationBrokenState(
      tab,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// Test if disabled due to flakiness http://crbug.com/368280 .
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ui_test_utils::NavigateToURL(browser(),
      https_server_mismatched_.GetURL("files/ssl/google.html"));

  // We get an interstitial page as a result.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // Now we try again with the right host name this time.
  GURL url(https_server_.GetURL("files/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Security state should be OK.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Now try again the broken one to make sure it is still broken.
  ui_test_utils::NavigateToURL(browser(),
      https_server_mismatched_.GetURL("files/ssl/google.html"));

  // Since we OKed the interstitial last time, we get right to the page.
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);
}

#if defined(OS_CHROMEOS)
// This test seems to be flaky and hang on chromiumos.
// http://crbug.com/84419
#define MAYBE_TestRefNavigation DISABLED_TestRefNavigation
#else
#define MAYBE_TestRefNavigation TestRefNavigation
#endif

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/page_with_refs.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/page_with_refs.html#jp"));

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
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

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  // It is probably overkill to add a notification for a popup-opening, let's
  // just poll.
  for (int i = 0; i < 10; i++) {
    if (IsShowingWebContentsModalDialog())
      break;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::MessageLoop::QuitClosure(),
        base::TimeDelta::FromSeconds(1));
    content::RunMessageLoop();
  }
  ASSERT_TRUE(IsShowingWebContentsModalDialog());

  // Let's add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = test_server()->GetURL("files/ssl/google.html");
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Close the first tab.
  chrome::CloseWebContents(browser(), tab1, false);
}

// Visit a page over bad https that is a redirect to a page with good https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_expired_.GetURL("server-redirect?");
  GURL url2 = https_server_.GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // We have been redirected to the good page.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Flaky on Linux. http://crbug.com/368280.
#if defined(OS_LINUX)
#define MAYBE_TestRedirectGoodToBadHTTPS DISABLED_TestRedirectGoodToBadHTTPS
#else
#define MAYBE_TestRedirectGoodToBadHTTPS TestRedirectGoodToBadHTTPS
#endif

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("server-redirect?");
  GURL url2 = https_server_expired_.GetURL("files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with good HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToGoodHTTPS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = test_server()->GetURL("server-redirect?");
  GURL good_https_url =
      https_server_.GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + good_https_url.spec()));
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Flaky on Linux. http://crbug.com/368280.
#if defined(OS_LINUX)
#define MAYBE_TestRedirectHTTPToBadHTTPS DISABLED_TestRedirectHTTPToBadHTTPS
#else
#define MAYBE_TestRedirectHTTPToBadHTTPS TestRedirectHTTPToBadHTTPS
#endif

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL http_url = test_server()->GetURL("server-redirect?");
  GURL bad_https_url =
      https_server_expired_.GetURL("files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("server-redirect?");
  GURL http_url = test_server()->GetURL("files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(https_url.spec() + http_url.spec()));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17"));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);

  // Same thing over HTTPS.
  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17"));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect insecure content), then back
IN_PROC_BROWSER_TEST_F(SSLUITest, TestGoodFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(top_frame_path));

  CheckAuthenticatedState(tab, AuthState::NONE);

  bool success = false;
  // Now navigate inside the frame.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be fine.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Now let's hit a bad page.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // The security style should still be secure.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // And the frame should be blocked.
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
        tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("window.domAutomationController.send("
                         "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame,
                                                   is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should still be OK.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a page served over HTTP.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('HTTPLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // Our state should be unathenticated (in the ran mixed script sense)
  CheckAuthenticationBrokenState(
      tab,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);

  // Go back, our state should be unchanged.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }

  CheckAuthenticationBrokenState(
      tab,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadFrameNavigation) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(top_frame_path));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // Navigate to a good frame.
  bool success = false;
  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  ASSERT_TRUE(success);
  observer.Wait();

  // We should still be authentication broken.
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
// Disabled, flakily exceeds test timeout, http://crbug.com/43437.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestUnauthenticatedFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  ASSERT_TRUE(GetTopFramePath(*test_server(),
                              https_server_,
                              https_server_expired_,
                              &top_frame_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(top_frame_path));
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate inside the frame to a secure HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be unauthenticated.
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate to a bad HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // State should not have changed.
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
        tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("window.domAutomationController.send("
                         "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame,
                                                   is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsInWorkerFiltered) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  std::string page_with_unsafe_worker_path;
  ASSERT_TRUE(GetPageWithUnsafeWorkerPath(https_server_expired_,
                                          &page_with_unsafe_worker_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      page_with_unsafe_worker_path));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // Expect Worker not to load insecure content.
  CheckWorkerLoadResult(tab, false);
  // The bad content is filtered, expect the state to be authenticated.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsInWorker) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to an unsafe site. Proceed with interstitial page to indicate
  // the user approves the bad certificate.
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/blank_page.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  std::string page_with_unsafe_worker_path;
  ASSERT_TRUE(GetPageWithUnsafeWorkerPath(https_server_expired_,
                                          &page_with_unsafe_worker_path));
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      page_with_unsafe_worker_path));
  CheckWorkerLoadResult(tab, true);  // Worker loads insecure content
  CheckAuthenticationBrokenState(
      tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// Test that when the browser blocks displaying insecure content (images), the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureImage) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Test that when the browser blocks displaying insecure content (iframes), the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means)
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureIframe) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_displays_insecure_iframe.html",
      test_server()->host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Test that when the browser blocks running insecure content, the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockRunningInsecureContent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/ssl/page_runs_insecure_content.html",
      test_server()->host_port_pair(),
      &replacement_path));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors. The connection should be established without
// interstitial page showing.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrors, TestWSS) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_expired_.GetURL(
          "connect_check.html").ReplaceComponents(replacements));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(LowerCaseEqualsASCII(result, "pass"));
}

// Verifies that the interstitial can proceed, even if JavaScript is disabled.
// http://crbug.com/322948
#if defined(OS_LINUX)
// flaky http://crbug.com/396458
#define MAYBE_TestInterstitialJavaScriptProceeds \
    DISABLED_TestInterstitialJavaScriptProceeds
#else
#define MAYBE_TestInterstitialJavaScriptProceeds \
    TestInterstitialJavaScriptProceeds
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestInterstitialJavaScriptProceeds) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->GetController()));
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetRenderViewHostForTesting();
  int result = -1;
  std::string javascript = base::StringPrintf(
      "window.domAutomationController.send(%d);",
      SSLBlockingPage::CMD_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
              interstitial_rvh, javascript, &result));
  // The above will hang without the fix.
  EXPECT_EQ(1, result);
  observer.Wait();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Verifies that the interstitial can go back, even if JavaScript is disabled.
// http://crbug.com/322948
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptGoesBack) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
      https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      content::NotificationService::AllSources());
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetRenderViewHostForTesting();
  int result = -1;
  std::string javascript = base::StringPrintf(
      "window.domAutomationController.send(%d);",
      SSLBlockingPage::CMD_DONT_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial_rvh, javascript, &result));
  // The above will hang without the fix.
  EXPECT_EQ(0, result);
  observer.Wait();
  EXPECT_EQ("about:blank", tab->GetVisibleURL().spec());
}

// Verifies that switching tabs, while showing interstitial page, will not
// affect the visibility of the interestitial.
// https://crbug.com/381439
IN_PROC_BROWSER_TEST_F(SSLUITest, InterstitialNotAffectedByHideShow) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("files/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());

  AddTabAtIndex(0,
                https_server_.GetURL("files/ssl/google.html"),
                ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_FALSE(tab->GetRenderWidgetHostView()->IsShowing());

  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.

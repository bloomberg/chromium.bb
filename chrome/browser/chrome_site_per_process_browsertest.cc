// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/display_switches.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/test/spellcheck_content_browser_client.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif

using app_modal::JavaScriptAppModalDialog;

namespace {

class RedirectObserver : public content::WebContentsObserver {
 public:
  explicit RedirectObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        transition_(ui::PageTransition::PAGE_TRANSITION_LINK) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;
    transition_ = navigation_handle->GetPageTransition();
    redirects_ = navigation_handle->GetRedirectChain();
  }

  void WebContentsDestroyed() override {
    // Make sure we don't close the tab while the observer is in scope.
    // See http://crbug.com/314036.
    FAIL() << "WebContents closed during navigation (http://crbug.com/314036).";
  }

  ui::PageTransition transition() const { return transition_; }
  const std::vector<GURL> redirects() const { return redirects_; }

 private:
  ui::PageTransition transition_;
  std::vector<GURL> redirects_;

  DISALLOW_COPY_AND_ASSIGN(RedirectObserver);
};

}  // namespace

class ChromeSitePerProcessTest : public InProcessBrowserTest {
 public:
  ChromeSitePerProcessTest() {}
  ~ChromeSitePerProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());

    // Serve from the root so that flash_object.html can load the swf file.
    // Needed for the PluginWithRemoteTopFrame test.
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSitePerProcessTest);
};

class SitePerProcessHighDPIExpiredCertBrowserTest
    : public ChromeSitePerProcessTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIExpiredCertBrowserTest()
      : https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  }

  net::EmbeddedTestServer* expired_cert_test_server() {
    return &https_server_expired_;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSitePerProcessTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }

  void SetUpOnMainThread() override {
    ChromeSitePerProcessTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server_expired_.Start());
  }

 private:
  net::EmbeddedTestServer https_server_expired_;
};

double GetFrameDeviceScaleFactor(const content::ToRenderFrameHost& adapter) {
  double device_scale_factor;
  const char kGetFrameDeviceScaleFactor[] =
      "window.domAutomationController.send(window.devicePixelRatio);";
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(adapter, kGetFrameDeviceScaleFactor,
                                            &device_scale_factor));
  return device_scale_factor;
}

// Flaky on Windows 10. http://crbug.com/700150
#if defined(OS_WIN)
#define MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor \
  DISABLED_InterstitialLoadsWithCorrectDeviceScaleFactor
#else
#define MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor \
  InterstitialLoadsWithCorrectDeviceScaleFactor
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIExpiredCertBrowserTest,
                       MAYBE_InterstitialLoadsWithCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  EXPECT_EQ(SitePerProcessHighDPIExpiredCertBrowserTest::kDeviceScaleFactor,
            GetFrameDeviceScaleFactor(
                browser()->tab_strip_model()->GetActiveWebContents()));

  // Navigate to page with expired cert.
  GURL bad_cert_url(
      expired_cert_test_server()->GetURL("c.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), bad_cert_url);
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitialAttach(active_web_contents);
  EXPECT_TRUE(active_web_contents->ShowingInterstitialPage());

  // Here we check the device scale factor in use via the interstitial's
  // RenderFrameHost; doing the check directly via the 'active web contents'
  // does not give us the device scale factor for the interstitial.
  content::RenderFrameHost* interstitial_frame_host =
      active_web_contents->GetInterstitialPage()->GetMainFrame();

  EXPECT_EQ(SitePerProcessHighDPIExpiredCertBrowserTest::kDeviceScaleFactor,
            GetFrameDeviceScaleFactor(interstitial_frame_host));
}

// Verify that browser shutdown path works correctly when there's a
// RenderFrameProxyHost for a child frame.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, RenderFrameProxyHostShutdown) {
  GURL main_url(embedded_test_server()->GetURL(
        "a.com",
        "/frame_tree/page_with_two_frames_remote_and_local.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
}

// Verify that origin replication allows JS access to localStorage, database,
// and FileSystem APIs.  These features involve a check on the
// WebSecurityOrigin of the topmost WebFrame in ContentSettingsObserver, and
// this test ensures this check works when the top frame is remote.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       OriginReplicationAllowsAccessToStorage) {
  // Navigate to a page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate subframe cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", cross_site_url));

  // Find the subframe's RenderFrameHost.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(cross_site_url, frame_host->GetLastCommittedURL());
  EXPECT_TRUE(frame_host->IsCrossProcessSubframe());

  // Check that JS storage APIs can be accessed successfully.
  EXPECT_TRUE(
      content::ExecuteScript(frame_host, "localStorage['foo'] = 'bar'"));
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame_host, "window.domAutomationController.send(localStorage['foo']);",
      &result));
  EXPECT_EQ(result, "bar");
  bool is_object_created = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      frame_host,
      "window.domAutomationController.send(!!indexedDB.open('testdb', 2));",
      &is_object_created));
  EXPECT_TRUE(is_object_created);
  is_object_created = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      frame_host,
      "window.domAutomationController.send(!!openDatabase("
      "'foodb', '1.0', 'Test DB', 1024));",
      &is_object_created));
  EXPECT_TRUE(is_object_created);
  EXPECT_TRUE(ExecuteScript(frame_host,
                            "window.webkitRequestFileSystem("
                            "window.TEMPORARY, 1024, function() {});"));
}

// Ensure that creating a plugin in a cross-site subframe doesn't crash.  This
// involves querying content settings from the renderer process and using the
// top frame's origin as one of the parameters.  See https://crbug.com/426658.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PluginWithRemoteTopFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/chrome/test/data/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate subframe to a page with a Flash object.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url =
      embedded_test_server()->GetURL("b.com",
                                     "/chrome/test/data/flash_object.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));
}

// Check that window.focus works for cross-process popups.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PopupWindowFocus) {
  GURL main_url(embedded_test_server()->GetURL("/page_with_focus_events.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Set window.name on main page.  This will be used to identify the page
  // later when it sends messages from its focus/blur events.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents, "window.name = 'main'"));

  // Open a popup for a cross-site page.
  GURL popup_url =
      embedded_test_server()->GetURL("foo.com", "/page_with_focus_events.html");
  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(ExecuteScript(web_contents,
                            "openPopup('" + popup_url.spec() + "','popup')"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(WaitForLoadStop(popup));
  EXPECT_EQ(popup_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Switch focus to the original tab, since opening a popup also focused it.
  web_contents->GetDelegate()->ActivateContents(web_contents);
  EXPECT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());

  // Focus the popup via window.focus().
  content::DOMMessageQueue queue;
  ExecuteScriptAsync(web_contents, "focusPopup()");

  // Wait for main page to lose focus and for popup to gain focus.  Each event
  // will send a message, and the two messages can arrive in any order.
  std::string status;
  bool main_lost_focus = false;
  bool popup_got_focus = false;
  while (queue.WaitForMessage(&status)) {
    if (status == "\"main-lost-focus\"")
      main_lost_focus = true;
    if (status == "\"popup-got-focus\"")
      popup_got_focus = true;
    if (main_lost_focus && popup_got_focus)
      break;
  }

  // The popup should be focused now.
  EXPECT_EQ(popup, browser()->tab_strip_model()->GetActiveWebContents());
}

// Verify that ctrl-click of an anchor targeting a remote frame works (i.e. that
// it opens the link in a new tab).  See also https://crbug.com/647772.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       AnchorCtrlClickWhenTargetIsCrossSite) {
  // Navigate to anchor_targeting_remote_frame.html.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/anchor_targeting_remote_frame.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Verify that there is only 1 active tab (with the right contents committed).
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(main_url, main_contents->GetLastCommittedURL());

  // Ctrl-click the anchor/link in the page.
  content::WebContentsAddedObserver new_tab_observer;
#if defined(OS_MACOSX)
  std::string new_tab_click_script = "simulateClick({ metaKey: true });";
#else
  std::string new_tab_click_script = "simulateClick({ ctrlKey: true });";
#endif
  EXPECT_TRUE(ExecuteScript(main_contents, new_tab_click_script));

  // Wait for a new tab to appear (the whole point of this test).
  content::WebContents* new_contents = new_tab_observer.GetWebContents();

  // Verify that the new tab has the right contents and is in the right, new
  // place in the tab strip.
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(new_contents, browser()->tab_strip_model()->GetWebContentsAt(1));
  GURL expected_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_EQ(expected_url, new_contents->GetLastCommittedURL());
}

class ChromeSitePerProcessPDFTest : public ChromeSitePerProcessTest {
 public:
  ChromeSitePerProcessPDFTest() : test_guest_view_manager_(nullptr) {}
  ~ChromeSitePerProcessPDFTest() override {}

  void SetUpOnMainThread() override {
    ChromeSitePerProcessTest::SetUpOnMainThread();
    guest_view::GuestViewManager::set_factory_for_testing(&factory_);
    test_guest_view_manager_ = static_cast<guest_view::TestGuestViewManager*>(
        guest_view::GuestViewManager::CreateWithDelegate(
            browser()->profile(),
            extensions::ExtensionsAPIClient::Get()
                ->CreateGuestViewManagerDelegate(browser()->profile())));
  }

 protected:
  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  guest_view::TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSitePerProcessPDFTest);
};

// This test verifies that when navigating an OOPIF to a page with <embed>-ed
// PDF, the guest is properly created, and by removing the embedder frame, the
// guest is properly destroyed (https://crbug.com/649856).
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessPDFTest,
                       EmbeddedPDFInsideCrossOriginFrame) {
  // Navigate to a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Initially, no guests are created.
  EXPECT_EQ(0U, test_guest_view_manager()->num_guests_created());

  // Navigate subframe to a cross-site page with an embedded PDF.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url =
      embedded_test_server()->GetURL("b.com", "/page_with_embedded_pdf.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Wait until the guest for PDF is created.
  content::WebContents* guest_web_contents =
      test_guest_view_manager()->WaitForSingleGuestCreated();

  // Now detach the frame and observe that the guest is destroyed.
  content::WebContentsDestroyedWatcher observer(guest_web_contents);
  EXPECT_TRUE(ExecuteScript(
      active_web_contents,
      "document.body.removeChild(document.querySelector('iframe'));"));
  observer.Wait();
  EXPECT_EQ(0U, test_guest_view_manager()->GetNumGuestsActive());
}

// A helper class to verify that a "mailto:" external protocol request succeeds.
class MailtoExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  bool has_triggered_external_protocol() {
    return has_triggered_external_protocol_;
  }

  const GURL& external_protocol_url() { return external_protocol_url_; }

  content::WebContents* web_contents() { return web_contents_; }

  void RunExternalProtocolDialog(const GURL& url,
                                 int render_process_host_id,
                                 int routing_id,
                                 ui::PageTransition page_transition,
                                 bool has_user_gesture) override {}

  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    return new shell_integration::DefaultProtocolClientWorker(callback,
                                                              protocol);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::DONT_BLOCK;
  }

  void BlockRequest() override {}

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    external_protocol_url_ = url;
    web_contents_ = web_contents;
    has_triggered_external_protocol_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  void Wait() {
    if (!has_triggered_external_protocol_) {
      message_loop_runner_ = new content::MessageLoopRunner();
      message_loop_runner_->Run();
    }
  }

  void FinishedProcessingCheck() override {}

 private:
  bool has_triggered_external_protocol_ = false;
  GURL external_protocol_url_;
  content::WebContents* web_contents_ = nullptr;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// This test is not run on ChromeOS because it registers a custom handler (see
// ProtocolHandlerRegistry::InstallDefaultsForChromeOS), and handles mailto:
// navigations before getting to external protocol code.
#if defined(OS_CHROMEOS)
#define MAYBE_LaunchExternalProtocolFromSubframe \
  DISABLED_LaunchExternalProtocolFromSubframe
#else
#define MAYBE_LaunchExternalProtocolFromSubframe \
  LaunchExternalProtocolFromSubframe
#endif
// This test verifies that external protocol requests succeed when made from an
// OOPIF (https://crbug.com/668289).
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       MAYBE_LaunchExternalProtocolFromSubframe) {
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  ui_test_utils::NavigateToURL(browser(), start_url);

  // Navigate to a page with a cross-site iframe that triggers a mailto:
  // external protocol request.
  // The test did not start by navigating to this URL because that would mask
  // the bug.  Instead, navigating the main frame to another page will cause a
  // cross-process transfer, which will avoid a situation where the OOPIF's
  // swapped-out RenderViewHost and the main frame's active RenderViewHost get
  // the same routing IDs, causing an accidental success.
  GURL mailto_main_frame_url(
      embedded_test_server()->GetURL("b.com", "/iframe.html"));

  ui_test_utils::NavigateToURL(browser(), mailto_main_frame_url);

  MailtoExternalProtocolHandlerDelegate delegate;
  ChromeResourceDispatcherHostDelegate::
      SetExternalProtocolHandlerDelegateForTesting(&delegate);

  GURL mailto_subframe_url(
      embedded_test_server()->GetURL("c.com", "/page_with_mailto.html"));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      NavigateIframeToURL(active_web_contents, "test", mailto_subframe_url));

  delegate.Wait();

  EXPECT_TRUE(delegate.has_triggered_external_protocol());
  EXPECT_EQ(delegate.external_protocol_url(), GURL("mailto:mail@example.org"));
  EXPECT_EQ(active_web_contents, delegate.web_contents());
  ChromeResourceDispatcherHostDelegate::
      SetExternalProtocolHandlerDelegateForTesting(nullptr);
}

// Verify that a popup can be opened after navigating a remote frame.  This has
// to be a chrome/ test to ensure that the popup blocker doesn't block the
// popup.  See https://crbug.com/670770.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       NavigateRemoteFrameAndOpenPopup) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate the iframe cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Run a script in the parent frame to (1) navigate iframe to another URL,
  // and (2) open a popup.  Note that ExecuteScript will run this with a user
  // gesture, so both steps should succeed.
  frame_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  bool popup_handle_is_valid = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents,
      "document.querySelector('iframe').src = '" + frame_url.spec() + "';\n"
      "var w = window.open('about:blank');\n"
      "window.domAutomationController.send(!!w);\n",
      &popup_handle_is_valid));
  popup_observer.Wait();

  // The popup shouldn't be blocked.
  EXPECT_TRUE(popup_handle_is_valid);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

// Ensure that a transferred cross-process navigation does not generate
// DidStopLoading events until the navigation commits.  If it did, then
// ui_test_utils::NavigateToURL would proceed before the URL had committed.
// http://crbug.com/243957.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       NoStopDuringTransferUntilCommit) {
  GURL init_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), init_url);

  // Navigate to a same-site page that redirects, causing a transfer.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a RedirectObserver that goes away before we close the tab.
  {
    RedirectObserver redirect_observer(contents);
    GURL dest_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
    GURL redirect_url(embedded_test_server()->GetURL(
        "c.com", "/server-redirect?" + dest_url.spec()));
    ui_test_utils::NavigateToURL(browser(), redirect_url);

    // We should immediately see the new committed entry.
    EXPECT_FALSE(contents->GetController().GetPendingEntry());
    EXPECT_EQ(dest_url,
              contents->GetController().GetLastCommittedEntry()->GetURL());

    // We should keep track of the original request URL, redirect chain, and
    // page transition type during a transfer, since these are necessary for
    // history autocomplete to work.
    EXPECT_EQ(redirect_url, contents->GetController()
                                .GetLastCommittedEntry()
                                ->GetOriginalRequestURL());
    EXPECT_EQ(2U, redirect_observer.redirects().size());
    EXPECT_EQ(redirect_url, redirect_observer.redirects().at(0));
    EXPECT_EQ(dest_url, redirect_observer.redirects().at(1));
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(redirect_observer.transition(),
                                             ui::PAGE_TRANSITION_TYPED));
  }
}

// Tests that a cross-process redirect will only cause the beforeunload
// handler to run once.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       SingleBeforeUnloadAfterRedirect) {
  // Navigate to a page with a beforeunload handler.
  GURL url(embedded_test_server()->GetURL("a.com", "/beforeunload.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to a URL that redirects to another process and approve the
  // beforeunload dialog that pops up.
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  GURL dest_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "c.com", "/server-redirect?" + dest_url.spec()));
  browser()->OpenURL(content::OpenURLParams(redirect_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->is_before_unload_dialog());
  alert->native_dialog()->AcceptAppModalDialog();
  nav_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PrintIgnoredInUnloadHandler) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("a.com", "/title1.html")));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create 2 iframes and navigate them to b.com.
  EXPECT_TRUE(ExecuteScript(active_web_contents,
                            "var i = document.createElement('iframe'); i.id = "
                            "'child-0'; document.body.appendChild(i);"));
  EXPECT_TRUE(ExecuteScript(active_web_contents,
                            "var i = document.createElement('iframe'); i.id = "
                            "'child-1'; document.body.appendChild(i);"));
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-0",
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html"))));
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-1",
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html"))));

  content::RenderFrameHost* child_0 =
      ChildFrameAt(active_web_contents->GetMainFrame(), 0);
  content::RenderFrameHost* child_1 =
      ChildFrameAt(active_web_contents->GetMainFrame(), 1);

  // Add an unload handler that calls print() to child-0 iframe.
  EXPECT_TRUE(ExecuteScript(
      child_0, "document.body.onunload = function() { print(); }"));

  // Transfer child-0 to a new process hosting c.com.
  EXPECT_TRUE(NavigateIframeToURL(
      active_web_contents, "child-0",
      GURL(embedded_test_server()->GetURL("c.com", "/title1.html"))));

  // Check that b.com's process is still alive.
  bool renderer_alive = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child_1, "window.domAutomationController.send(true);", &renderer_alive));
  EXPECT_TRUE(renderer_alive);
}

// Ensure that when a window closes itself via window.close(), its process does
// not get destroyed if there's a pending cross-process navigation in the same
// process from another tab.  See https://crbug.com/799399.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       ClosePopupWithPendingNavigationInOpener) {
  // Start on a.com and open a popup to b.com.
  GURL opener_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), opener_url);
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(ExecuteScript(opener_contents,
                            "window.open('" + popup_url.spec() + "');"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(opener_contents, popup_contents);
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));

  // From the popup, start a navigation in the opener to b.com, but don't
  // commit.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  content::TestNavigationManager manager(opener_contents, b_url);
  EXPECT_TRUE(
      ExecuteScript(popup_contents, "opener.location='" + b_url.spec() + "';"));

  // Close the popup.  This should *not* kill the b.com process, as it still
  // has a pending navigation in the opener window.
  content::RenderProcessHost* b_com_rph =
      popup_contents->GetMainFrame()->GetProcess();
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecuteScript(popup_contents, "window.close();"));
  destroyed_watcher.Wait();
  EXPECT_TRUE(b_com_rph->HasConnection());

  // Resume the pending navigation in the original tab and ensure it finishes
  // loading successfully.
  manager.WaitForNavigationFinished();
  EXPECT_EQ(b_url, opener_contents->GetMainFrame()->GetLastCommittedURL());
}

#if BUILDFLAG(ENABLE_SPELLCHECK)
// Class to sniff incoming spellcheck IPC / Mojo SpellCheckHost messages.
class TestSpellCheckMessageFilter : public content::BrowserMessageFilter,
                                    spellcheck::mojom::SpellCheckHost {
 public:
  explicit TestSpellCheckMessageFilter(content::RenderProcessHost* process_host)
      : content::BrowserMessageFilter(SpellCheckMsgStart),
        process_host_(process_host),
        text_received_(false),
        message_loop_runner_(
            base::MakeRefCounted<content::MessageLoopRunner>()),
        binding_(this) {}

  content::RenderProcessHost* process_host() const { return process_host_; }

  const base::string16& text() const { return text_; }

  bool HasReceivedText() const { return text_received_; }

  void Wait() {
    if (!text_received_)
      message_loop_runner_->Run();
  }

  void WaitUntilTimeout() {
    if (text_received_)
      return;
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        message_loop_runner_->QuitClosure(), base::TimeDelta::FromSeconds(1));
    message_loop_runner_->Run();
  }

  bool OnMessageReceived(const IPC::Message& message) override {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    IPC_BEGIN_MESSAGE_MAP(TestSpellCheckMessageFilter, message)
      // TODO(crbug.com/714480): convert the RequestTextCheck IPC to mojo.
      IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestTextCheck, HandleMessage)
    IPC_END_MESSAGE_MAP()
#endif
    return false;
  }

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void SpellCheckHostRequest(spellcheck::mojom::SpellCheckHostRequest request) {
    EXPECT_FALSE(binding_.is_bound());
    binding_.Bind(std::move(request));
  }
#endif

 private:
  ~TestSpellCheckMessageFilter() override {}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void HandleMessage(int, int, const base::string16& text) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&TestSpellCheckMessageFilter::HandleMessageOnUIThread,
                       this, text));
  }
#endif

  void HandleMessageOnUIThread(const base::string16& text) {
    if (!text_received_) {
      text_received_ = true;
      text_ = text;
      message_loop_runner_->Quit();
    } else {
      NOTREACHED();
    }
  }

  // spellcheck::mojom::SpellCheckHost:
  void RequestDictionary() override {}

  void NotifyChecked(const base::string16& word, bool misspelled) override {}

  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::move(callback).Run(true, std::vector<SpellCheckResult>());
    binding_.Close();
    HandleMessageOnUIThread(text);
#endif
  }

  content::RenderProcessHost* process_host_;
  bool text_received_;
  base::string16 text_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  mojo::Binding<spellcheck::mojom::SpellCheckHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestSpellCheckMessageFilter);
};

class TestBrowserClientForSpellCheck : public ChromeContentBrowserClient {
 public:
  TestBrowserClientForSpellCheck() = default;

  // ContentBrowserClient overrides.
  void RenderProcessWillLaunch(
      content::RenderProcessHost* process_host) override {
    filters_.push_back(new TestSpellCheckMessageFilter(process_host));
    process_host->AddFilter(filters_.back().get());
    ChromeContentBrowserClient::RenderProcessWillLaunch(process_host);
  }

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OverrideOnBindInterface(
      const service_manager::BindSourceInfo& remote_info,
      const std::string& name,
      mojo::ScopedMessagePipeHandle* handle) override {
    if (name != spellcheck::mojom::SpellCheckHost::Name_)
      return;

    spellcheck::mojom::SpellCheckHostRequest request(std::move(*handle));

    // Override the default SpellCheckHost interface.
    auto ui_task_runner = content::BrowserThread::GetTaskRunnerForThread(
        content::BrowserThread::UI);
    ui_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TestBrowserClientForSpellCheck::BindSpellCheckHostRequest,
            base::Unretained(this), base::Passed(&request), remote_info));
  }

#endif  // !BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // Retrieves the registered filter for the given RenderProcessHost. It will
  // return nullptr if the RenderProcessHost was initialized while a different
  // instance of ContentBrowserClient was in action.
  scoped_refptr<TestSpellCheckMessageFilter>
  GetSpellCheckMessageFilterForProcess(
      content::RenderProcessHost* process_host) const {
    for (auto filter : filters_) {
      if (filter->process_host() == process_host)
        return filter;
    }
    return nullptr;
  }

 private:
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void BindSpellCheckHostRequest(
      spellcheck::mojom::SpellCheckHostRequest request,
      const service_manager::BindSourceInfo& source_info) {
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromRendererIdentity(source_info.identity);
    scoped_refptr<TestSpellCheckMessageFilter> filter =
        GetSpellCheckMessageFilterForProcess(host);
    CHECK(filter);
    filter->SpellCheckHostRequest(std::move(request));
  }
#endif

  std::vector<scoped_refptr<TestSpellCheckMessageFilter>> filters_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserClientForSpellCheck);
};

// Tests that spelling in out-of-process subframes is checked.
// See crbug.com/638361 for details.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, OOPIFSpellCheckTest) {
  TestBrowserClientForSpellCheck browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* cross_site_subframe =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  scoped_refptr<TestSpellCheckMessageFilter> filter =
      browser_client.GetSpellCheckMessageFilterForProcess(
          cross_site_subframe->GetProcess());
  filter->Wait();

  EXPECT_EQ(base::ASCIIToUTF16("zz."), filter->text());

  content::SetBrowserClientForTesting(old_browser_client);
}

// Tests that after disabling spellchecking, spelling in new out-of-process
// subframes is not checked. See crbug.com/789273 for details.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, OOPIFDisabledSpellCheckTest) {
  TestBrowserClientForSpellCheck browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  content::BrowserContext* browser_context =
      static_cast<content::BrowserContext*>(browser()->profile());

  // Initiate a SpellcheckService
  SpellcheckServiceFactory::GetForContext(browser_context);

  // Disable spellcheck
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
  base::RunLoop().RunUntilIdle();

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* cross_site_subframe =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  scoped_refptr<TestSpellCheckMessageFilter> filter =
      browser_client.GetSpellCheckMessageFilterForProcess(
          cross_site_subframe->GetProcess());
  filter->WaitUntilTimeout();

  // Shouldn't receive text since spellchecking is disabled.
  EXPECT_FALSE(filter->HasReceivedText());

  content::SetBrowserClientForTesting(old_browser_client);
  prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
}

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)

// Tests that the OSX spell check panel can be opened from an out-of-process
// subframe, crbug.com/712395
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, OOPIFSpellCheckPanelTest) {
  spellcheck::SpellCheckContentBrowserClient browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* cross_site_subframe =
      ChildFrameAt(web_contents->GetMainFrame(), 0);

  EXPECT_TRUE(cross_site_subframe->IsCrossProcessSubframe());

  spellcheck::mojom::SpellCheckPanelPtr spell_check_panel_client;
  cross_site_subframe->GetRemoteInterfaces()->GetInterface(
      &spell_check_panel_client);
  spell_check_panel_client->ToggleSpellPanel(false);
  browser_client.RunUntilBind();

  spellcheck::SpellCheckMockPanelHost* host =
      browser_client.GetSpellCheckMockPanelHostForProcess(
          cross_site_subframe->GetProcess());
  EXPECT_TRUE(host->SpellingPanelVisible());

  content::SetBrowserClientForTesting(old_browser_client);
}
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if defined(USE_AURA)
// Test that with a desktop/laptop touchscreen, a two finger tap opens a
// context menu in an OOPIF.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, TwoFingerTapContextMenu) {
  // Start on a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate the iframe cross-site.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", frame_url));

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  content::RenderWidgetHostView* child_rwhv = child_frame->GetView();
  content::RenderWidgetHost* child_rwh = child_rwhv->GetRenderWidgetHost();

  ASSERT_TRUE(child_frame->IsCrossProcessSubframe());

  // Send a two finger tap event to the child and wait for the context menu to
  // open.
  ContextMenuWaiter menu_waiter(content::NotificationService::AllSources());

  gfx::Point child_location(1, 1);
  gfx::Point child_location_in_root =
      child_rwhv->TransformPointToRootCoordSpace(child_location);

  blink::WebGestureEvent event(blink::WebInputEvent::kGestureTwoFingerTap,
                               blink::WebInputEvent::kNoModifiers,
                               blink::WebInputEvent::kTimeStampForTesting);
  event.source_device = blink::kWebGestureDeviceTouchscreen;
  event.x = child_location.x();
  event.y = child_location.y();
  event.global_x = child_location_in_root.x();
  event.global_y = child_location_in_root.y();
  event.data.two_finger_tap.first_finger_width = 10;
  event.data.two_finger_tap.first_finger_height = 10;

  child_rwh->ForwardGestureEvent(event);

  menu_waiter.WaitForMenuOpenAndClose();
}
#endif  // defined(USE_AURA)

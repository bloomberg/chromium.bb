// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/display_switches.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif

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
  EXPECT_TRUE(ExecuteScript(web_contents, "focusPopup()"));

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

  void Wait() {
    if (!text_received_)
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
  void ShellCheckHostRequest(const service_manager::BindSourceInfo& source_info,
                             spellcheck::mojom::SpellCheckHostRequest request) {
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
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      content::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override {
    // Expose the default interfaces.
    ChromeContentBrowserClient::ExposeInterfacesToRenderer(
        registry, associated_registry, render_process_host);

    scoped_refptr<TestSpellCheckMessageFilter> filter =
        GetSpellCheckMessageFilterForProcess(render_process_host);
    CHECK(filter);

    // Override the default SpellCheckHost interface.
    auto ui_task_runner = content::BrowserThread::GetTaskRunnerForThread(
        content::BrowserThread::UI);
    registry->AddInterface(
        base::Bind(&TestSpellCheckMessageFilter::ShellCheckHostRequest, filter),
        ui_task_runner);
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

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
class TestSpellCheckPanelHost : public spellcheck::mojom::SpellCheckPanelHost {
 public:
  explicit TestSpellCheckPanelHost(content::RenderProcessHost* process_host)
      : process_host_(process_host) {}

  content::RenderProcessHost* process_host() const { return process_host_; }

  bool SpellingPanelVisible() {
    if (!show_spelling_panel_called_) {
      base::RunLoop run_loop;
      quit_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    EXPECT_TRUE(show_spelling_panel_called_);
    return spelling_panel_visible_;
  }

  void SpellCheckPanelHostRequest(
      const service_manager::BindSourceInfo& source_info,
      spellcheck::mojom::SpellCheckPanelHostRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  // spellcheck::mojom::SpellCheckPanelHost:
  void ShowSpellingPanel(bool show) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    show_spelling_panel_called_ = true;
    spelling_panel_visible_ = show;
    if (quit_)
      std::move(quit_).Run();
  }

  void UpdateSpellingPanelWithMisspelledWord(
      const base::string16& word) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  mojo::BindingSet<spellcheck::mojom::SpellCheckPanelHost> bindings_;
  content::RenderProcessHost* process_host_;
  bool show_spelling_panel_called_ = false;
  bool spelling_panel_visible_ = false;
  base::OnceClosure quit_;

  DISALLOW_COPY_AND_ASSIGN(TestSpellCheckPanelHost);
};

class TestBrowserClientForSpellCheckPanelHost
    : public ChromeContentBrowserClient {
 public:
  TestBrowserClientForSpellCheckPanelHost() = default;

  // ContentBrowserClient overrides.
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      content::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override {
    hosts_.push_back(
        base::MakeUnique<TestSpellCheckPanelHost>(render_process_host));

    // Expose the default interfaces.
    ChromeContentBrowserClient::ExposeInterfacesToRenderer(
        registry, associated_registry, render_process_host);

    // Override the default SpellCheckPanelHost interface.
    auto ui_task_runner = content::BrowserThread::GetTaskRunnerForThread(
        content::BrowserThread::UI);
    registry->AddInterface(
        base::Bind(&TestSpellCheckPanelHost::SpellCheckPanelHostRequest,
                   base::Unretained(hosts_.back().get())),
        ui_task_runner);
  }

  TestSpellCheckPanelHost* GetTestSpellCheckPanelHostForProcess(
      content::RenderProcessHost* render_process_host) const {
    for (const auto& host : hosts_) {
      if (host->process_host() == render_process_host)
        return host.get();
    }
    return nullptr;
  }

 private:
  std::vector<std::unique_ptr<TestSpellCheckPanelHost>> hosts_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserClientForSpellCheckPanelHost);
};

// Tests that the OSX spell check panel can be opened from an out-of-process
// subframe, crbug.com/712395
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, OOPIFSpellCheckPanelTest) {
  TestBrowserClientForSpellCheckPanelHost browser_client;
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

  TestSpellCheckPanelHost* host =
      browser_client.GetTestSpellCheckPanelHostForProcess(
          cross_site_subframe->GetProcess());
  EXPECT_TRUE(host->SpellingPanelVisible());

  content::SetBrowserClientForTesting(old_browser_client);
}
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/command_line.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

std::string CreateClientRedirect(const std::string& dest_url) {
  const char* const kClientRedirectBase = "client-redirect?";
  return kClientRedirectBase + dest_url;
}

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "server-redirect?";
  return kServerRedirectBase + dest_url;
}

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const GURL& referrer,
      int number_of_loads,
      FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, profile, url, referrer),
        number_of_loads_(0),
        expected_number_of_loads_(number_of_loads),
        expected_final_status_(expected_final_status),
        new_render_view_host_(NULL),
        was_hidden_(false),
        was_shown_(false) {
  }

  virtual ~TestPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status()) <<
        " when testing URL " << prerender_url().path();
    // Prerendering RenderViewHosts should be hidden before the first
    // navigation, so this should be happen for every PrerenderContents for
    // while a RenderViewHost is created, regardless of whether or not it's
    // used.
    if (new_render_view_host_) {
      EXPECT_TRUE(was_hidden_);
    }

    // A used PrerenderContents will only be destroyed when we swap out
    // TabContents, at the end of a navigation caused by a call to
    // NavigateToURLImpl().
    if (final_status() == FINAL_STATUS_USED) {
      EXPECT_TRUE(new_render_view_host_);
      EXPECT_TRUE(was_shown_);
    } else {
      EXPECT_FALSE(was_shown_);
    }

    // When the PrerenderContents is destroyed, quit the UI message loop.
    // This happens on navigation to used prerendered pages, and soon
    // after cancellation of unused prerendered pages.
    MessageLoopForUI::current()->Quit();
  }

  virtual void OnRenderViewGone(int status, int exit_code) OVERRIDE {
    // On quit, it's possible to end up here when render processes are closed
    // before the PrerenderManager is destroyed.  As a result, it's possible to
    // get either FINAL_STATUS_APP_TERMINATING or FINAL_STATUS_RENDERER_CRASHED
    // on quit.
    if (expected_final_status_ == FINAL_STATUS_APP_TERMINATING)
      expected_final_status_ = FINAL_STATUS_RENDERER_CRASHED;

    PrerenderContents::OnRenderViewGone(status, exit_code);
  }

  virtual void DidStopLoading() OVERRIDE {
    PrerenderContents::DidStopLoading();
    ++number_of_loads_;
    if (expected_final_status_ == FINAL_STATUS_USED &&
        number_of_loads_ >= expected_number_of_loads_) {
      MessageLoopForUI::current()->Quit();
    } else if (expected_final_status_ == FINAL_STATUS_RENDERER_CRASHED) {
      // Crash the render process.  This has to be done here because
      // a prerender navigating to about:crash is cancelled with
      // "FINAL_STATUS_HTTPS".  Even if this were worked around,
      // about:crash can't be navigated to by a normal webpage.
      render_view_host_mutable()->NavigateToURL(GURL("about:crash"));
    }
  }

 private:
  virtual void OnRenderViewHostCreated(
      RenderViewHost* new_render_view_host) OVERRIDE {
    // Used to make sure the RenderViewHost is hidden and, if used,
    // subsequently shown.
    notification_registrar().Add(
        this,
        NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
        Source<RenderWidgetHost>(new_render_view_host));

    new_render_view_host_ = new_render_view_host;

    PrerenderContents::OnRenderViewHostCreated(new_render_view_host);
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    if (type.value ==
        NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED) {
      EXPECT_EQ(new_render_view_host_, Source<RenderWidgetHost>(source).ptr());
      bool is_visible = *Details<bool>(details).ptr();

      if (!is_visible) {
        was_hidden_ = true;
      } else if (is_visible && was_hidden_) {
        // Once hidden, a prerendered RenderViewHost should only be shown after
        // being removed from the PrerenderContents for display.
        EXPECT_FALSE(render_view_host());
        was_shown_ = true;
      }
      return;
    }
    PrerenderContents::Observe(type, source, details);
  }

  int number_of_loads_;
  int expected_number_of_loads_;
  FinalStatus expected_final_status_;

  // The RenderViewHost created for the prerender, if any.
  RenderViewHost* new_render_view_host_;
  // Set to true when the prerendering RenderWidget is hidden.
  bool was_hidden_;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_;
};

// PrerenderManager that uses TestPrerenderContents.
class WaitForLoadPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  WaitForLoadPrerenderContentsFactory(
      int number_of_loads,
      const std::deque<FinalStatus>& expected_final_status_queue)
      : number_of_loads_(number_of_loads) {
    expected_final_status_queue_.resize(expected_final_status_queue.size());
    std::copy(expected_final_status_queue.begin(),
              expected_final_status_queue.end(),
              expected_final_status_queue_.begin());
    VLOG(1) << "Factory created with queue length " <<
               expected_final_status_queue_.size();
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const GURL& referrer) OVERRIDE {
    CHECK(!expected_final_status_queue_.empty()) <<
          "Creating prerender contents for " << url.path() <<
          " with no expected final status";
    FinalStatus expected_final_status = expected_final_status_queue_.front();
    expected_final_status_queue_.pop_front();
    VLOG(1) << "Creating prerender contents for " << url.path() <<
               " with expected final status " << expected_final_status;
    VLOG(1) << expected_final_status_queue_.size() << " left in the queue.";
    return new TestPrerenderContents(prerender_manager, profile, url,
                                     referrer, number_of_loads_,
                                     expected_final_status);
  }

 private:
  int number_of_loads_;
  std::deque<FinalStatus> expected_final_status_queue_;
};

}  // namespace

class PrerenderBrowserTest : public InProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_contents_factory_(NULL),
        use_https_src_server_(false),
        call_javascript_(true) {
    EnableDOMAutomation();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kPrerender,
                                    switches::kPrerenderSwitchValueEnabled);
#if defined(OS_MACOSX)
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    FilePath app_dir;
    PathService::Get(chrome::DIR_APP, &app_dir);
    command_line->AppendSwitchPath(
        switches::kExtraPluginDir,
        app_dir.Append(FILE_PATH_LITERAL("plugins")));
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // TODO(mmenke):  Once downloading is stopped earlier, remove this.
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);

    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
  }

  // Overload for a single expected final status
  void PrerenderTestURL(const std::string& html_file,
                        FinalStatus expected_final_status,
                        int total_navigations) {
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURL(html_file,
                     expected_final_status_queue,
                     total_navigations);
  }

  void PrerenderTestURL(
      const std::string& html_file,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int total_navigations) {
    ASSERT_TRUE(test_server()->Start());
    PrerenderTestURLImpl(test_server()->GetURL(html_file),
                         expected_final_status_queue,
                         total_navigations);
  }

  void PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int total_navigations) {
    ASSERT_TRUE(test_server()->Start());
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURLImpl(url, expected_final_status_queue, total_navigations);
  }

  void NavigateToDestURL() const {
    NavigateToURLImpl(dest_url_);
  }

  // Should be const but test_server()->GetURL(...) is not const.
  void NavigateToURL(const std::string& dest_html_file) {
    GURL dest_url = test_server()->GetURL(dest_html_file);
    NavigateToURLImpl(dest_url);
  }

  bool UrlIsInPrerenderManager(const std::string& html_file) {
    GURL dest_url = test_server()->GetURL(html_file);
    return (prerender_manager()->FindEntry(dest_url) != NULL);
  }

  bool UrlIsInPrerenderManager(const GURL& url) {
    return (prerender_manager()->FindEntry(url) != NULL);
  }

  bool UrlIsPendingInPrerenderManager(const std::string& html_file) {
    GURL dest_url = test_server()->GetURL(html_file);
    return (prerender_manager()->FindPendingEntry(dest_url) != NULL);
  }

  void set_use_https_src(bool use_https_src_server) {
    use_https_src_server_ = use_https_src_server;
  }

  void DisableJavascriptCalls() {
    call_javascript_ = false;
  }

  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }

  PrerenderManager* prerender_manager() const {
    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    return prerender_manager;
  }

 private:
  void PrerenderTestURLImpl(
      const GURL& url,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int total_navigations) {
    dest_url_ = url;

    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_PREFETCH_URL", dest_url_.spec()));
    std::string replacement_path;
    ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
        "files/prerender/prerender_loader.html",
        replacement_text,
        &replacement_path));

    net::TestServer* src_server = test_server();
    scoped_ptr<net::TestServer> https_src_server;
    if (use_https_src_server_) {
      https_src_server.reset(
          new net::TestServer(net::TestServer::TYPE_HTTPS,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
      ASSERT_TRUE(https_src_server->Start());
      src_server = https_src_server.get();
    }
    GURL src_url = src_server->GetURL(replacement_path);

    // This is needed to exit the event loop once the prerendered page has
    // stopped loading or was cancelled.
    ASSERT_TRUE(prerender_manager());
    prerender_manager()->rate_limit_enabled_ = false;
    ASSERT_TRUE(prerender_contents_factory_ == NULL);
    prerender_contents_factory_ =
        new WaitForLoadPrerenderContentsFactory(total_navigations,
                                                expected_final_status_queue);
    prerender_manager()->SetPrerenderContentsFactory(
        prerender_contents_factory_);
    FinalStatus expected_final_status = expected_final_status_queue.front();

    // ui_test_utils::NavigateToURL uses its own observer and message loop.
    // Since the test needs to wait until the prerendered page has stopped
    // loading, rather than the page directly navigated to, need to
    // handle browser navigation directly.
    browser()->OpenURL(src_url, GURL(), CURRENT_TAB, PageTransition::TYPED);

    TestPrerenderContents* prerender_contents = NULL;
    ui_test_utils::RunMessageLoop();

    prerender_contents =
        static_cast<TestPrerenderContents*>(
            prerender_manager()->FindEntry(dest_url_));

    switch (expected_final_status) {
      case FINAL_STATUS_USED: {
        ASSERT_TRUE(prerender_contents != NULL);

        if (call_javascript_) {
          // Check if page behaves as expected while in prerendered state.
          bool prerender_test_result = false;
          ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
              prerender_contents->render_view_host_mutable(), L"",
              L"window.domAutomationController.send(DidPrerenderPass())",
              &prerender_test_result));
          EXPECT_TRUE(prerender_test_result);
        }
        break;
      }
      default:
        // In the failure case, we should have removed dest_url_ from the
        // prerender_manager.
        EXPECT_TRUE(prerender_contents == NULL);
        break;
    }
  }

  void NavigateToURLImpl(const GURL& dest_url) const {
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    EXPECT_TRUE(prerender_manager()->FindEntry(dest_url_) != NULL);

    // ui_test_utils::NavigateToURL waits until DidStopLoading is called on
    // the current tab.  As that tab is going to end up deleted, and may never
    // finish loading before that happens, exit the message loop on the deletion
    // of the used prerender contents instead.
    //
    // As PrerenderTestURL waits until the prerendered page has completely
    // loaded, there is no race between loading |dest_url| and swapping the
    // prerendered TabContents into the tab.
    browser()->OpenURL(dest_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
    ui_test_utils::RunMessageLoop();

    // Make sure the PrerenderContents found earlier was used or removed.
    EXPECT_TRUE(prerender_manager()->FindEntry(dest_url_) == NULL);

    if (call_javascript_) {
      // Check if page behaved as expected when actually displayed.
      bool display_test_result = false;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
          browser()->GetSelectedTabContents()->render_view_host(), L"",
          L"window.domAutomationController.send(DidDisplayPass())",
          &display_test_result));
      EXPECT_TRUE(display_test_result);
    }
  }

  WaitForLoadPrerenderContentsFactory* prerender_contents_factory_;
  GURL dest_url_;
  bool use_https_src_server_;
  bool call_javascript_;

  // Location of the downloads directory for these tests
  ScopedTempDir downloads_directory_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prefetch> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   1);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_after_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that plugins in an iframe are not loaded while a page is
// being preloaded, but are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderIframeDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/prerender_iframe_plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
// Flaky, and crbug.com was down when discovered, so no crbug entry.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderHttpAuthentication) {
  PrerenderTestURL("files/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED,
                   1);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToFirst) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToDestURL();
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecond) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that a prefetch for an https will prevent a prerender from happening.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_HTTPS,
                   1);
}

// Checks that client-issued redirects to an https page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClientRedirectToHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(CreateClientRedirect(https_url.spec()),
                   FINAL_STATUS_HTTPS,
                   1);
}

// Checks that client-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClientRedirectInIframe) {
  std::string redirect_path = CreateClientRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that client-issued redirects within an iframe in a prerendered
// page to an https page will not cancel the prerender, nor will it
// count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectToHttpsInIframe) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  std::string redirect_path = CreateClientRedirect(https_url.spec());
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(https_url));
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToFirst) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecond) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that server-issued redirects from an http to an https
// location will cancel prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectToHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(CreateServerRedirect(https_url.spec()),
                   FINAL_STATUS_HTTPS,
                   1);
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path = CreateServerRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that server-issued redirects within an iframe in a prerendered
// page to an https page will not cancel the prerender, nor will it
// count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectToHttpsInIframe) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  std::string redirect_path = CreateServerRedirect(https_url.spec());
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(https_url));
  NavigateToDestURL();
}

// Prerenders a page that contains an automatic download triggered through an
// iframe. This should not prerender successfully.
// Flaky: http://crbug.com/81985
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FLAKY_PrerenderDownloadIframe) {
  PrerenderTestURL("files/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully
// Flaky: http://crbug.com/81985
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       FLAKY_PrerenderDownloadLocation) {
  PrerenderTestURL(CreateClientRedirect("files/download-test1.lib"),
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through a
// client-issued redirect. This should not prerender successfully.
// Flaky: http://crbug.com/81985
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       FLAKY_PrerenderDownloadClientRedirect) {
  PrerenderTestURL("files/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("files/prerender/prerender_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  set_use_https_src(true);
  PrerenderTestURL("files/prerender/prerender_no_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that popups on a prerendered page cause cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPopup) {
  PrerenderTestURL("files/prerender/prerender_popup.html",
                   FINAL_STATUS_CREATE_NEW_WINDOW,
                   1);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(prerender_manager());
  prerender_manager()->set_max_prerender_memory_mb(30);
  PrerenderTestURL("files/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED,
                   1);
}

// Checks that we don't prerender in an infinite loop.
// crbug.com/83170
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FLAKY_PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "files/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "files/prerender/prerender_infinite_b.html";

  std::deque<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next url is now in the manager
  // and not pending.
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
}

// Checks that we don't prerender in an infinite loop and multiple links are
// handled correctly.
// Flaky, http://crbug.com/77323, but failing in a CHECK so disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderInfiniteLoopMultiple) {
  const char* const kHtmlFileA =
      "files/prerender/prerender_infinite_a_multiple.html";
  const char* const kHtmlFileB =
      "files/prerender/prerender_infinite_b_multiple.html";
  const char* const kHtmlFileC =
      "files/prerender/prerender_infinite_c_multiple.html";

  // We need to set the final status to expect here before starting any
  // prerenders. We set them on a queue so whichever we see first is expected to
  // be evicted, and the second should stick around until we exit.
  std::deque<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_EVICTED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileC));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileC));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next urls are now in the manager
  // and not pending. One and only one of the URLs (the last seen) should be the
  // active entry.
  bool url_b_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileB);
  bool url_c_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileC);
  EXPECT_TRUE((url_b_is_active_prerender || url_c_is_active_prerender) &&
              !(url_b_is_active_prerender && url_c_is_active_prerender));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileC));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderTaskManager) {
  // Early out if we're not using TabContents for Prerendering.
  if (!prerender::PrerenderContents::UseTabContents()) {
    SUCCEED();
    return;
  }

  // Show the task manager. This populates the model.
  browser()->window()->ShowTaskManager();

  // Start with two resources.
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // One of the resources that has a TabContents associated with it should have
  // the Prerender prefix.
  const string16 prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX, string16());
  string16 prerender_title;
  int num_prerender_tabs = 0;

  for (int i = 0; i < model()->ResourceCount(); ++i) {
    if (model()->GetResourceTabContents(i)) {
      prerender_title = model()->GetResourceTitle(i);
      if (StartsWith(prerender_title, prefix, true))
        ++num_prerender_tabs;
    }
  }
  EXPECT_EQ(1, num_prerender_tabs);
  const string16 prerender_page_title = prerender_title.substr(prefix.length());

  NavigateToDestURL();

  // There should be no tabs with the Prerender prefix.
  const string16 tab_prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX, string16());
  num_prerender_tabs = 0;
  int num_tabs_with_prerender_page_title = 0;
  for (int i = 0; i < model()->ResourceCount(); ++i) {
    if (model()->GetResourceTabContents(i)) {
      string16 tab_title = model()->GetResourceTitle(i);
      if (StartsWith(tab_title, prefix, true)) {
        ++num_prerender_tabs;
      } else {
        EXPECT_TRUE(StartsWith(tab_title, tab_prefix, true));

        // The prerender tab should now be a normal tab but the title should be
        // the same. Depending on timing, there may be more than one of these.
        const string16 tab_page_title = tab_title.substr(tab_prefix.length());
        if (prerender_page_title.compare(tab_page_title) == 0)
          ++num_tabs_with_prerender_page_title;
      }
    }
  }
  EXPECT_EQ(0, num_prerender_tabs);

  // We may have deleted the prerender tab, but the swapped in tab should be
  // active.
  EXPECT_GE(num_tabs_with_prerender_page_title, 1);
  EXPECT_LE(num_tabs_with_prerender_page_title, 2);
}

// Checks that prerenderers will terminate when an audio tag is encountered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Audio) {
  PrerenderTestURL("files/prerender/prerender_html5_audio.html",
                   FINAL_STATUS_HTML5_MEDIA,
                   1);
}

// Checks that prerenderers will terminate when a video tag is encountered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Video) {
  PrerenderTestURL("files/prerender/prerender_html5_video.html",
                   FINAL_STATUS_HTML5_MEDIA,
                   1);
}

// Checks that prerenderers will terminate when a video tag is inserted via
// javascript.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5VideoJs) {
  PrerenderTestURL("files/prerender/prerender_html5_video_script.html",
                   FINAL_STATUS_HTML5_MEDIA,
                   1);
}

// Checks that scripts can retrieve the correct window size while prerendering.
#if defined(TOOLKIT_VIEWS)
// TODO(beng): Widget hierarchy split causes this to fail http://crbug.com/82363
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderWindowSize) {
#else
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderWindowSize) {
#endif
  PrerenderTestURL("files/prerender/prerender_size.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that prerenderers will terminate when the RenderView crashes.
// Note that the prerendering RenderView will be redirected to about:crash.
// Disabled, http://crbug.com/80561
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderRendererCrash) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_RENDERER_CRASHED,
                   1);
}

// Checks that we correctly use a prerendered page when navigating to a
// fragment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNavigateFragment) {
  PrerenderTestURL("files/prerender/prerender_fragment.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_fragment.html#fragment");
}

// Checks that we correctly use a prerendered page when we prerender a fragment
// but navigate to the main page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFragmentNavigatePage) {
  PrerenderTestURL("files/prerender/prerender_page.html#fragment",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that we correctly use a prerendered page when we prerender a fragment
// but navigate to a different fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderFragmentNavigateFragment) {
  PrerenderTestURL("files/prerender/prerender_fragment.html#other_fragment",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_fragment.html#fragment");
}

// Checks that we correctly use a prerendered page when the page uses meta
// http-equiv to refresh to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectToFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_fragment.html#fragment"),
      FINAL_STATUS_USED,
      2);
  NavigateToURL(
      CreateClientRedirect("files/prerender/prerender_fragment.html"));
}

// Checks that we correctly use a prerendered page when the page uses JS to set
// the window.location.hash to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageChangeFragmentLocationHash) {
  PrerenderTestURL("files/prerender/prerender_fragment_location_hash.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_fragment_location_hash.html");
}

// Checks that prerendering a PNG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImagePng) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.png", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerendering a JPG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImageJpeg) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.jpeg", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that a prerender of a CRX will result in a cancellation due to
// download.
// Flaky: http://crbug.com/81985
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FLAKY_PrerenderCrx) {
  PrerenderTestURL("files/prerender/extension.crx", FINAL_STATUS_DOWNLOAD, 1);
}

// Checks that xhr GET requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrGet) {
  PrerenderTestURL("files/prerender/prerender_xhr_get.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr HEAD requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrHead) {
  PrerenderTestURL("files/prerender/prerender_xhr_head.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr OPTIONS requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrOptions) {
  PrerenderTestURL("files/prerender/prerender_xhr_options.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr TRACE requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrTrace) {
  PrerenderTestURL("files/prerender/prerender_xhr_trace.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr POST cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPost) {
  PrerenderTestURL("files/prerender/prerender_xhr_post.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that xhr PUT cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPut) {
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that xhr DELETE cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrDelete) {
  PrerenderTestURL("files/prerender/prerender_xhr_delete.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that a top-level page which would trigger an SSL error is simply
// canceled. Note that this happens before checking the cert, since an https
// URL should not be sent out.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorTopLevel) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_HTTPS,
                   1);
}

// Checks that an SSL error that comes from a subresource does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorSubresource) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/image.jpeg");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that an SSL error that comes from an iframe does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorIframe) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(
      "files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that we cancel correctly when window.print() is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPrint) {
  PrerenderTestURL("files/prerender/prerender_print.html",
                   FINAL_STATUS_WINDOW_PRINT,
                   1);
}

}  // namespace prerender

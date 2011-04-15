// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

#include <deque>

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
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls,
      const GURL& referrer,
      int number_of_loads,
      FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, profile, url, alias_urls,
                          referrer),
        number_of_loads_(0),
        expected_number_of_loads_(number_of_loads),
        expected_final_status_(expected_final_status) {
  }

  virtual ~TestPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status()) <<
        " when testing URL " << prerender_url().path();
    // In the event we are destroyed, say if the prerender was canceled, quit
    // the UI message loop.
    MessageLoopForUI::current()->Quit();
  }

  virtual void DidStopLoading() {
    PrerenderContents::DidStopLoading();
    ++number_of_loads_;
    if (expected_final_status_ == FINAL_STATUS_USED &&
        number_of_loads_ >= expected_number_of_loads_) {
      MessageLoopForUI::current()->Quit();
    }
  }

 private:
  int number_of_loads_;
  int expected_number_of_loads_;
  FinalStatus expected_final_status_;
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
    LOG(INFO) << "Factory created with queue length " <<
                 expected_final_status_queue_.size();
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls, const GURL& referrer) {
    CHECK(!expected_final_status_queue_.empty()) <<
          "Creating prerender contents for " << url.path() <<
          " with no expected final status";
    FinalStatus expected_final_status = expected_final_status_queue_.front();
    expected_final_status_queue_.pop_front();
    LOG(INFO) << "Creating prerender contents for " << url.path() <<
                 " with expected final status " << expected_final_status;
    LOG(INFO) << expected_final_status_queue_.size() << " left in the queue.";
    return new TestPrerenderContents(prerender_manager, profile, url,
                                     alias_urls, referrer,
                                     number_of_loads_,
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
      : prc_factory_(NULL),
        use_https_src_server_(false) {
    EnableDOMAutomation();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
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

  // Overload for a single expected final status
  void PrerenderTestURL(const std::string& html_file,
                        FinalStatus expected_final_status,
                        int total_navigations) {
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURLImpl(html_file,
                         expected_final_status_queue,
                         total_navigations);
  }

  void PrerenderTestURL(
      const std::string& html_file,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int total_navigations) {
    PrerenderTestURLImpl(html_file,
                         expected_final_status_queue,
                         total_navigations);
  }

  void NavigateToDestURL() const {
    ui_test_utils::NavigateToURL(browser(), dest_url_);

    // Make sure the PrerenderContents found earlier was used or removed
    EXPECT_TRUE(prerender_manager()->FindEntry(dest_url_) == NULL);

    // Check if page behaved as expected when actually displayed.
    bool display_test_result = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send(DidDisplayPass())",
        &display_test_result));
    EXPECT_TRUE(display_test_result);
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

  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }

  void set_dest_url(const GURL& dest_url) { dest_url_ = dest_url; }

 private:
  void PrerenderTestURLImpl(
      const std::string& html_file,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int total_navigations) {
    ASSERT_TRUE(test_server()->Start());
    dest_url_ = test_server()->GetURL(html_file);

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
    ASSERT_TRUE(prc_factory_ == NULL);
    prc_factory_ =
        new WaitForLoadPrerenderContentsFactory(total_navigations,
                                                expected_final_status_queue);
    prerender_manager()->SetPrerenderContentsFactory(prc_factory_);
    FinalStatus expected_final_status = expected_final_status_queue.front();

    // ui_test_utils::NavigateToURL uses its own observer and message loop.
    // Since the test needs to wait until the prerendered page has stopped
    // loading, rathather than the page directly navigated to, need to
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

        // Check if page behaves as expected while in prerendered state.
        bool prerender_test_result = false;
        ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
            prerender_contents->render_view_host(), L"",
            L"window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result));
        EXPECT_TRUE(prerender_test_result);
        break;
      }
      default:
        // In the failure case, we should have removed dest_url_ from the
        // prerender_manager.
        EXPECT_TRUE(prerender_contents == NULL);
        break;
    }
  }

  PrerenderManager* prerender_manager() const {
    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    return prerender_manager;
  }

  WaitForLoadPrerenderContentsFactory* prc_factory_;
  GURL dest_url_;
  bool use_https_src_server_;
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
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("files/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED,
                   1);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToFirst) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_page.html"),
      FINAL_STATUS_USED,
      2);
  NavigateToDestURL();
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecond) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_page.html"),
      FINAL_STATUS_USED,
      2);
  set_dest_url(test_server()->GetURL("files/prerender/prerender_page.html"));
  NavigateToDestURL();
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
  PrerenderTestURL(
      CreateServerRedirect("files/prerender/prerender_page.html"),
      FINAL_STATUS_USED,
      1);
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecond) {
  std::string redirect_path;
  PrerenderTestURL(
      CreateServerRedirect("files/prerender/prerender_page.html"),
      FINAL_STATUS_USED,
      1);
  set_dest_url(test_server()->GetURL("files/prerender/prerender_page.html"));
  NavigateToDestURL();
}

// TODO(cbentzel): Add server-redirect-to-https test. http://crbug.com/79182

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
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadIFrame) {
  PrerenderTestURL("files/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadLocation) {
  PrerenderTestURL(CreateClientRedirect("files/download-test1.lib"),
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through a
// client-issued redirect. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadClientRedirect) {
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
// Disabled, http://crbug.com/77870.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderExcessiveMemory) {
  PrerenderTestURL("files/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED,
                   1);
}

// Checks that we don't prerender in an infinite loop.
// Disabled, http://crbug.com/77870.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "prerender_infinite_a.html";
  const char* const kHtmlFileB = "prerender_infinite_b.html";

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
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       FLAKY_PrerenderInfiniteLoopMultiple) {
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

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TaskManager) {
  // Show the task manager. This populates the model.
  browser()->window()->ShowTaskManager();

  // Start with two resources.
  EXPECT_EQ(2, model()->ResourceCount());
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // The prerender makes three.
  EXPECT_EQ(3, model()->ResourceCount());

  // It shouldn't have a TabContents associated with it.
  ASSERT_TRUE(model()->GetResourceTabContents(1) == NULL);

  // The prefix should be "Prerender:"
  string16 prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX,
                                 string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(1), prefix, true));

  NavigateToDestURL();

  // Prerender task should be killed and removed from the Task Manager.
  EXPECT_EQ(2, model()->ResourceCount());
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

}  // namespace prerender

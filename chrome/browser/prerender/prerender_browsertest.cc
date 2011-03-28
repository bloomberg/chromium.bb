// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/url_request/url_request_context.h"

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

bool CreateRedirect(const std::string& dest_url, std::string* redirect_path) {
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(make_pair("REPLACE_WITH_URL", dest_url));
  return net::TestServer::GetFilePathWithReplacements(
      "prerender_redirect.html",
      replacement_text,
      redirect_path);
}

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls,
      const GURL& referrer,
      FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, profile, url, alias_urls,
                          referrer),
        did_finish_loading_(false),
        expected_final_status_(expected_final_status) {
  }

  virtual ~TestPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status()) <<
        " when testing URL " << prerender_url().path();
    // In the event we are destroyed, say if the prerender was canceled, quit
    // the UI message loop.
    if (!did_finish_loading_)
      MessageLoopForUI::current()->Quit();
  }

  virtual void DidStopLoading() {
    PrerenderContents::DidStopLoading();
    did_finish_loading_ = true;
    MessageLoopForUI::current()->Quit();
  }

  bool did_finish_loading() const { return did_finish_loading_; }
  void set_did_finish_loading(bool did_finish_loading) {
    did_finish_loading_ = did_finish_loading;
  }

 private:
  bool did_finish_loading_;
  FinalStatus expected_final_status_;
};

// PrerenderManager that uses TestPrerenderContents.
class WaitForLoadPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  explicit WaitForLoadPrerenderContentsFactory(
      FinalStatus expected_final_status)
      : expected_final_status_(expected_final_status) {
  }

  void set_expected_final_status(FinalStatus expected_final_status) {
    expected_final_status_ = expected_final_status;
  }

  void set_expected_final_status_for_url(const GURL& url,
                                         FinalStatus expected_final_status) {
    DCHECK(expected_final_status_map_.find(url) ==
           expected_final_status_map_.end());
    expected_final_status_map_[url] = expected_final_status;
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls, const GURL& referrer) {
    FinalStatus expected_final_status = expected_final_status_;
    std::map<GURL, FinalStatus>::iterator it =
        expected_final_status_map_.find(url);
    if (it != expected_final_status_map_.end()) {
      expected_final_status = it->second;
      expected_final_status_map_.erase(it);
    }
    return new TestPrerenderContents(prerender_manager, profile, url,
                                     alias_urls, referrer,
                                     expected_final_status);
  }

 private:
  FinalStatus expected_final_status_;
  std::map<GURL, FinalStatus> expected_final_status_map_;
};

}  // namespace

class PrerenderBrowserTest : public InProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : prc_factory_(NULL),
        use_https_src_server_(false),
        on_iteration_succeeded_(true) {
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

  void PrerenderTestURL(const std::string& html_file,
                        FinalStatus expected_final_status,
                        int total_navigations) {
    ASSERT_TRUE(test_server()->Start());
    dest_url_ = UrlForHtmlFile(html_file);

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
        new WaitForLoadPrerenderContentsFactory(expected_final_status);
    prerender_manager()->SetPrerenderContentsFactory(prc_factory_);

    // ui_test_utils::NavigateToURL uses its own observer and message loop.
    // Since the test needs to wait until the prerendered page has stopped
    // loading, rathather than the page directly navigated to, need to
    // handle browser navigation directly.
    browser()->OpenURL(src_url, GURL(), CURRENT_TAB, PageTransition::TYPED);

    TestPrerenderContents* prerender_contents = NULL;
    int navigations = 0;
    while (true) {
      ui_test_utils::RunMessageLoop();
      ++navigations;
      EXPECT_TRUE(on_iteration_succeeded_);

      prerender_contents =
          static_cast<TestPrerenderContents*>(
              prerender_manager()->FindEntry(dest_url_));
      if (prerender_contents == NULL ||
          !prerender_contents->did_finish_loading() ||
          navigations >= total_navigations) {
        EXPECT_EQ(navigations, total_navigations);
        break;
      }
      prerender_contents->set_did_finish_loading(false);
      MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          NewRunnableMethod(this,
                            &PrerenderBrowserTest::CallOnIteration,
                            prerender_contents->render_view_host()));
    }

    switch (expected_final_status) {
      case FINAL_STATUS_USED: {
        ASSERT_TRUE(prerender_contents != NULL);
        ASSERT_TRUE(prerender_contents->did_finish_loading());

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
    GURL dest_url = UrlForHtmlFile(html_file);

    return (prerender_manager()->FindEntry(dest_url) != NULL);
  }

  bool UrlIsPendingInPrerenderManager(const std::string& html_file) {
    GURL dest_url = UrlForHtmlFile(html_file);

    return (prerender_manager()->FindPendingEntry(dest_url) != NULL);
  }

  void set_use_https_src(bool use_https_src_server) {
    use_https_src_server_ = use_https_src_server;
  }

  void SetExpectedFinalStatus(FinalStatus expected_final_status) {
    DCHECK(prerender_manager()->prerender_contents_factory_.get() ==
           prc_factory_);
    prc_factory_->set_expected_final_status(expected_final_status);
  }

  void SetExpectedFinalStatusForUrl(const std::string& html_file,
                                    FinalStatus expected_final_status) {
    GURL url = UrlForHtmlFile(html_file);
    DCHECK(prerender_manager()->prerender_contents_factory_.get() ==
           prc_factory_);
    prc_factory_->set_expected_final_status_for_url(url, expected_final_status);
  }

 private:
  PrerenderManager* prerender_manager() const {
    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    return prerender_manager;
  }

  // Non-const as test_server()->GetURL() is not const
  GURL UrlForHtmlFile(const std::string& html_file) {
    std::string dest_path = "files/prerender/";
    dest_path.append(html_file);
    return test_server()->GetURL(dest_path);
  }

  void CallOnIteration(RenderViewHost* rvh) {
    on_iteration_succeeded_ = ui_test_utils::ExecuteJavaScript(
        rvh,
        L"",
        L"if (typeof(OnIteration) != 'undefined') {OnIteration();}");
  }

  WaitForLoadPrerenderContentsFactory* prc_factory_;
  GURL dest_url_;
  bool use_https_src_server_;
  bool on_iteration_succeeded_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prefetch> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  PrerenderTestURL("prerender_page.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL("prerender_alert_before_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT, 1);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL("prerender_alert_after_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT, 1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDelayLoadPlugin) {
  PrerenderTestURL("plugin_delay_load.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that plugins in an iframe are not loaded while a page is
// being preloaded, but are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderIframeDelayLoadPlugin) {
  PrerenderTestURL("prerender_iframe_plugin_delay_load.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED, 1);
}

// Checks that HTML redirects work with prerendering - specifically, checks the
// page is used and plugins aren't loaded.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRedirect) {
  std::string redirect_path;
  ASSERT_TRUE(CreateRedirect("prerender_page.html", &redirect_path));
  PrerenderTestURL(redirect_path,
                   FINAL_STATUS_USED, 2);
  NavigateToDestURL();
}

// Prerenders a page that contains an automatic download triggered through an
// iframe. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadIFrame) {
  PrerenderTestURL("prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD, 1);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadLocation) {
  std::string redirect_path;
  ASSERT_TRUE(CreateRedirect("../download-test1.lib", &redirect_path));
  PrerenderTestURL(redirect_path, FINAL_STATUS_DOWNLOAD, 2);
}

// Prerenders a page that contains an automatic download triggered through a
// <meta http-equiv="refresh"> tag. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadRefresh) {
  PrerenderTestURL("prerender_download_refresh.html",
                   FINAL_STATUS_DOWNLOAD, 2);
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("prerender_referrer.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  set_use_https_src(true);
  PrerenderTestURL("prerender_no_referrer.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that popups on a prerendered page cause cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPopup) {
  PrerenderTestURL("prerender_popup.html",
                   FINAL_STATUS_CREATE_NEW_WINDOW, 1);
}

// Test that page-based redirects to https will cancel prerenders.
// Disabled, http://crbug.com/73580
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderRedirectToHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  std::string redirect_path;
  ASSERT_TRUE(CreateRedirect(https_url.spec(), &redirect_path));
  PrerenderTestURL(redirect_path,
                   FINAL_STATUS_HTTPS,
                   2);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  PrerenderTestURL("prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED, 1);
}

// Checks that we don't prerender in an infinite loop.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "prerender_infinite_a.html";
  const char* const kHtmlFileB = "prerender_infinite_b.html";

  PrerenderTestURL(kHtmlFileA, FINAL_STATUS_USED, 1);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));

  // We are not going to navigate back to kHtmlFileA but we will start the
  // preload so we need to set the final status to expect here before
  // navigating.
  SetExpectedFinalStatus(FINAL_STATUS_APP_TERMINATING);

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next url is now in the manager
  // and not pending.
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
}

// Checks that we don't prerender in an infinite loop and multiple links are
// handled correctly.
// Flaky, http://crbug.com/77323
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FLAKY_PrerenderInfiniteLoopMultiple) {
  const char* const kHtmlFileA = "prerender_infinite_a_multiple.html";
  const char* const kHtmlFileB = "prerender_infinite_b_multiple.html";
  const char* const kHtmlFileC = "prerender_infinite_c_multiple.html";

  PrerenderTestURL(kHtmlFileA, FINAL_STATUS_USED, 1);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileC));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileC));

  // We are not going to navigate back to kHtmlFileA but we will start the
  // preload so we need to set the final status to expect here before
  // navigating.
  SetExpectedFinalStatusForUrl(kHtmlFileB, FINAL_STATUS_EVICTED);
  SetExpectedFinalStatusForUrl(kHtmlFileC, FINAL_STATUS_APP_TERMINATING);

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next urls are now in the manager
  // and not pending. url_c was the last seen so should be the active
  // entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileC));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileC));
}

}  // namespace prerender

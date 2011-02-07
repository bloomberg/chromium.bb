// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
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

namespace {

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls,
      PrerenderContents::FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, profile, url, alias_urls),
        did_finish_loading_(false),
        expected_final_status_(expected_final_status) {
  }

  virtual ~TestPrerenderContents() {
    EXPECT_EQ(expected_final_status_, final_status());
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
  PrerenderContents::FinalStatus expected_final_status_;
};

// PrerenderManager that uses TestPrerenderContents.
class WaitForLoadPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  explicit WaitForLoadPrerenderContentsFactory(
      PrerenderContents::FinalStatus expected_final_status)
      : expected_final_status_(expected_final_status) {
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls) {
    return new TestPrerenderContents(prerender_manager, profile, url,
                                     alias_urls, expected_final_status_);
  }

 private:
  PrerenderContents::FinalStatus expected_final_status_;
};

}  // namespace

class PrerenderBrowserTest : public InProcessBrowserTest {
 public:
  PrerenderBrowserTest() {
    EnableDOMAutomation();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePagePrerender);
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
                        PrerenderContents::FinalStatus expected_final_status,
                        int total_navigations) {
    ASSERT_TRUE(test_server()->Start());

    std::string src_path = "files/prerender/prerender_loader.html?";
    src_path.append(html_file);
    std::string dest_path = "files/prerender/";
    dest_path.append(html_file);

    GURL src_url = test_server()->GetURL(src_path);
    dest_url_ = test_server()->GetURL(dest_path);

    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    ASSERT_TRUE(prerender_manager);

    // This is needed to exit the event loop once the prerendered page has
    // stopped loading or was cancelled.
    prerender_manager->SetPrerenderContentsFactory(
        new WaitForLoadPrerenderContentsFactory(expected_final_status));

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

      prerender_contents =
          static_cast<TestPrerenderContents*>(
              prerender_manager->FindEntry(dest_url_));
      if (prerender_contents == NULL ||
          !prerender_contents->did_finish_loading() ||
          navigations >= total_navigations) {
        EXPECT_EQ(navigations, total_navigations);
        break;
      }
      prerender_contents->set_did_finish_loading(false);
    }

    switch (expected_final_status) {
      case PrerenderContents::FINAL_STATUS_USED: {
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

    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    ASSERT_TRUE(prerender_manager);

    // Make sure the PrerenderContents found earlier was used or removed
    EXPECT_TRUE(prerender_manager->FindEntry(dest_url_) == NULL);

    // Check if page behaved as expected when actually displayed.
    bool display_test_result = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send(DidDisplayPass())",
        &display_test_result));
    EXPECT_TRUE(display_test_result);
  }

 private:
  GURL dest_url_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prefetch> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  PrerenderTestURL("prerender_page.html",
                   PrerenderContents::FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL(
      "prerender_alert_before_onload.html",
      PrerenderContents::FINAL_STATUS_JAVASCRIPT_ALERT, 1);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL(
      "prerender_alert_after_onload.html",
      PrerenderContents::FINAL_STATUS_JAVASCRIPT_ALERT, 1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDelayLoadPlugin) {
  PrerenderTestURL("plugin_delay_load.html",
                   PrerenderContents::FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("prerender_http_auth_container.html",
                   PrerenderContents::FINAL_STATUS_AUTH_NEEDED, 1);
}

// Checks that redirects work with prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRedirect) {
  PrerenderTestURL("prerender_redirect.html",
                   PrerenderContents::FINAL_STATUS_USED, 2);
  NavigateToDestURL();
}

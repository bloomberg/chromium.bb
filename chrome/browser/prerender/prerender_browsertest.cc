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
      const std::vector<GURL>& alias_urls)
      : PrerenderContents(prerender_manager, profile, url, alias_urls),
        did_finish_loading_(false) {
  }

  virtual void DidStopLoading() {
    PrerenderContents::DidStopLoading();
    did_finish_loading_ = true;
    MessageLoopForUI::current()->Quit();
  }

  bool did_finish_loading() const { return did_finish_loading_; }

 private:
  bool did_finish_loading_;
};

// PrerenderManager that uses TestPrerenderContents.
class WaitForLoadPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls) {
    return new TestPrerenderContents(prerender_manager, profile, url,
                                     alias_urls);
  }
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

  void RunTestURL(const std::string& html_file) {
    ASSERT_TRUE(test_server()->Start());

    std::string src_path = "files/prerender/prerender_loader.html?";
    src_path.append(html_file);
    std::string dest_path = "files/prerender/";
    dest_path.append(html_file);

    GURL src_url = test_server()->GetURL(src_path);
    GURL dest_url = test_server()->GetURL(dest_path);

    Profile* profile = browser()->GetSelectedTabContents()->profile();
    PrerenderManager* prerender_manager = profile->GetPrerenderManager();
    ASSERT_TRUE(prerender_manager);

    // This is needed to exit the event loop once the prerendered page has
    // stopped loading.
    prerender_manager->SetPrerenderContentsFactory(
        new WaitForLoadPrerenderContentsFactory());

    // ui_test_utils::NavigateToURL uses its own observer and message loop.
    // Since the test needs to wait until the prerendered page has stopped
    // loading, rathather than the page directly navigated to, need to
    // handle browser navigation directly.
    browser()->OpenURL(src_url, GURL(), CURRENT_TAB, PageTransition::TYPED);

    ui_test_utils::RunMessageLoop();

    TestPrerenderContents* prerender_contents =
        static_cast<TestPrerenderContents*>(
            prerender_manager->FindEntry(dest_url));

    // Make sure the prefetech link was caught and the page was prerendered.
    ASSERT_TRUE(prerender_contents != NULL);
    ASSERT_TRUE(prerender_contents->did_finish_loading());

    // Check if page behaves as expected while in prerendered state.
    bool prerender_test_result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        prerender_contents->render_view_host(), L"",
        L"window.domAutomationController.send(DidPrerenderPass())",
        &prerender_test_result));
    EXPECT_TRUE(prerender_test_result);

    ui_test_utils::NavigateToURL(browser(), dest_url);

    // Make sure the PrerenderContents found earlier was used.
    EXPECT_TRUE(prerender_manager->FindEntry(dest_url) == NULL);

    // Check if page behaved as expected when actually displayed.
    bool display_test_result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send(DidDisplayPass())",
        &display_test_result));
    EXPECT_TRUE(display_test_result);
  }
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prefetch> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  RunTestURL("prerender_page.html");
}

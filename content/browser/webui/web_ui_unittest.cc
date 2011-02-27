// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebUITest : public RenderViewHostTestHarness {
 public:
  WebUITest() : ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

  // Tests navigating with a Web UI from a fresh (nothing pending or committed)
  // state, through pending, committed, then another navigation. The first page
  // ID that we should use is passed as a parameter. We'll use the next two
  // values. This must be increasing for the life of the tests.
  static void DoNavigationTest(TabContents* contents, int page_id) {
    NavigationController* controller = &contents->controller();

    // Start a pending load.
    GURL new_tab_url(chrome::kChromeUINewTabURL);
    controller->LoadURL(new_tab_url, GURL(), PageTransition::LINK);

    // The navigation entry should be pending with no committed entry.
    ASSERT_TRUE(controller->pending_entry());
    ASSERT_FALSE(controller->GetLastCommittedEntry());

    // Check the things the pending Web UI should have set.
    EXPECT_FALSE(contents->ShouldDisplayURL());
    EXPECT_FALSE(contents->ShouldDisplayFavIcon());
    EXPECT_TRUE(contents->ShouldShowBookmarkBar());
    EXPECT_TRUE(contents->FocusLocationBarByDefault());

    // Now commit the load.
    static_cast<TestRenderViewHost*>(
        contents->render_view_host())->SendNavigate(page_id, new_tab_url);

    // The same flags should be set as before now that the load has committed.
    EXPECT_FALSE(contents->ShouldDisplayURL());
    EXPECT_FALSE(contents->ShouldDisplayFavIcon());
    EXPECT_TRUE(contents->ShouldShowBookmarkBar());
    EXPECT_TRUE(contents->FocusLocationBarByDefault());

    // Start a pending navigation to a regular page.
    GURL next_url("http://google.com/");
    controller->LoadURL(next_url, GURL(), PageTransition::LINK);

    // Check the flags. Some should reflect the new page (URL, title), some
    // should reflect the old one (bookmark bar) until it has committed.
    EXPECT_TRUE(contents->ShouldDisplayURL());
    EXPECT_TRUE(contents->ShouldDisplayFavIcon());
    EXPECT_TRUE(contents->ShouldShowBookmarkBar());
    EXPECT_FALSE(contents->FocusLocationBarByDefault());

    // Commit the regular page load. Note that we must send it to the "pending"
    // RenderViewHost if there is one, since this transition will also cause a
    // process transition, and our RVH pointer will be the "committed" one.
    // In the second call to this function from WebUIToStandard, it won't
    // actually be pending, which is the point of this test.
    if (contents->render_manager()->pending_render_view_host()) {
      static_cast<TestRenderViewHost*>(
          contents->render_manager()->pending_render_view_host())->SendNavigate(
              page_id + 1, next_url);
    } else {
      static_cast<TestRenderViewHost*>(
          contents->render_view_host())->SendNavigate(page_id + 1, next_url);
    }

    // The state should now reflect a regular page.
    EXPECT_TRUE(contents->ShouldDisplayURL());
    EXPECT_TRUE(contents->ShouldDisplayFavIcon());
    EXPECT_FALSE(contents->ShouldShowBookmarkBar());
    EXPECT_FALSE(contents->FocusLocationBarByDefault());
  }

 private:
  BrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebUITest);
};

// Tests that the New Tab Page flags are correctly set and propogated by
// TabContents when we first navigate to a Web UI page, then to a standard
// non-DOM-UI page.
TEST_F(WebUITest, WebUIToStandard) {
  DoNavigationTest(contents(), 1);

  // Test the case where we're not doing the initial navigation. This is
  // slightly different than the very-first-navigation case since the
  // SiteInstance will be the same (the original TabContents must still be
  // alive), which will trigger different behavior in RenderViewHostManager.
  TestTabContents contents2(profile_.get(), NULL);

  DoNavigationTest(&contents2, 101);
}

TEST_F(WebUITest, WebUIToWebUI) {
  // Do a load (this state is tested above).
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller().LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(1, new_tab_url);

  // Start another pending load of the new tab page.
  controller().LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(2, new_tab_url);

  // The flags should be the same as the non-pending state.
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_FALSE(contents()->ShouldDisplayFavIcon());
  EXPECT_TRUE(contents()->ShouldShowBookmarkBar());
  EXPECT_TRUE(contents()->FocusLocationBarByDefault());
}

TEST_F(WebUITest, StandardToWebUI) {
  // Start a pending navigation to a regular page.
  GURL std_url("http://google.com/");

  controller().LoadURL(std_url, GURL(), PageTransition::LINK);

  // The state should now reflect the default.
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->ShouldShowBookmarkBar());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Commit the load, the state should be the same.
  rvh()->SendNavigate(1, std_url);
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->ShouldShowBookmarkBar());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Start a pending load for a WebUI.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller().LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->ShouldShowBookmarkBar());
  EXPECT_TRUE(contents()->FocusLocationBarByDefault());

  // Committing Web UI is tested above.
}

class TabContentsForFocusTest : public TestTabContents {
 public:
  TabContentsForFocusTest(Profile* profile, SiteInstance* instance)
      : TestTabContents(profile, instance), focus_called_(0) {
  }

  virtual void SetFocusToLocationBar(bool select_all) { ++focus_called_; }
  int focus_called() const { return focus_called_; }

 private:
  int focus_called_;
};

TEST_F(WebUITest, FocusOnNavigate) {
  // Setup.  |tc| will be used to track when we try to focus the location bar.
  TabContentsForFocusTest* tc = new TabContentsForFocusTest(
      contents()->profile(),
      SiteInstance::CreateSiteInstance(contents()->profile()));
  tc->controller().CopyStateFrom(controller());
  scoped_ptr<TestTabContents> tc_scoped_ptr(tc);
  contents_.swap(tc_scoped_ptr);
  int page_id = 200;

  // Load the NTP.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller().LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(page_id, new_tab_url);

  // Navigate to another page.
  GURL next_url("http://google.com/");
  int next_page_id = page_id + 1;
  controller().LoadURL(next_url, GURL(), PageTransition::LINK);
  pending_rvh()->SendNavigate(next_page_id, next_url);

  // Navigate back.  Should focus the location bar.
  int focus_called = tc->focus_called();
  ASSERT_TRUE(controller().CanGoBack());
  controller().GoBack();
  pending_rvh()->SendNavigate(page_id, new_tab_url);
  EXPECT_LT(focus_called, tc->focus_called());

  // Navigate forward.  Shouldn't focus the location bar.
  focus_called = tc->focus_called();
  ASSERT_TRUE(controller().CanGoForward());
  controller().GoForward();
  pending_rvh()->SendNavigate(next_page_id, next_url);
  EXPECT_EQ(focus_called, tc->focus_called());

  contents_.swap(tc_scoped_ptr);
}

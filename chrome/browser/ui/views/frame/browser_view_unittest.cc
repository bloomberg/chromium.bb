// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"
#include "ui/views/controls/single_split_view.h"
#include "ui/views/controls/webview/webview.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_frame_win.h"
#endif

namespace {

// Tab strip bounds depend on the window frame sizes.
gfx::Point ExpectedTabStripOrigin(BrowserView* browser_view) {
  gfx::Rect tabstrip_bounds(
      browser_view->frame()->GetBoundsForTabStrip(browser_view->tabstrip()));
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToTarget(browser_view->parent(),
                                    browser_view,
                                    &tabstrip_origin);
  return tabstrip_origin;
}

}  // namespace

typedef TestWithBrowserView BrowserViewTest;

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // The window is owned by the native widget, not the test class.
  EXPECT_FALSE(window());

  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  EXPECT_FALSE(browser_view()->IsOffTheRecord());
  EXPECT_EQ(IDR_OTR_ICON, browser_view()->GetOTRIconResourceID());
  EXPECT_FALSE(browser_view()->IsGuestSession());
  EXPECT_FALSE(browser_view()->ShouldShowAvatar());
  EXPECT_TRUE(browser_view()->IsBrowserTypeNormal());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarAnimating());
}

// Test layout of the top-of-window UI.
TEST_F(BrowserViewTest, BrowserViewLayout) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  // |browser_view_| owns the Browser, not the test class.
  Browser* browser = browser_view()->browser();
  TopContainerView* top_container = browser_view()->top_container();
  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::SingleSplitView* contents_split =
      browser_view()->GetContentsSplitForTest();
  views::WebView* contents_web_view =
      browser_view()->GetContentsWebViewForTest();

  // Start with a single tab open to a normal page.
  AddTab(browser, GURL("about:blank"));

  // Verify the view hierarchy.
  EXPECT_EQ(top_container, browser_view()->tabstrip()->parent());
  EXPECT_EQ(top_container, browser_view()->toolbar()->parent());
  EXPECT_EQ(top_container, browser_view()->GetBookmarkBarView()->parent());
  EXPECT_EQ(browser_view(), browser_view()->infobar_container()->parent());

  // Find bar host is at the front of the view hierarchy, followed by the top
  // container.
  EXPECT_EQ(browser_view()->child_count() - 1,
            browser_view()->GetIndexOf(browser_view()->find_bar_host_view()));
  EXPECT_EQ(browser_view()->child_count() - 2,
            browser_view()->GetIndexOf(top_container));

  // Verify basic layout.
  EXPECT_EQ(0, top_container->x());
  EXPECT_EQ(0, top_container->y());
  EXPECT_EQ(browser_view()->width(), top_container->width());
  // Tabstrip layout varies based on window frame sizes.
  gfx::Point expected_tabstrip_origin = ExpectedTabStripOrigin(browser_view());
  EXPECT_EQ(expected_tabstrip_origin.x(), tabstrip->x());
  EXPECT_EQ(expected_tabstrip_origin.y(), tabstrip->y());
  EXPECT_EQ(0, toolbar->x());
  EXPECT_EQ(
      tabstrip->bounds().bottom() -
          BrowserViewLayout::kToolbarTabStripVerticalOverlap,
      toolbar->y());
  EXPECT_EQ(0, contents_split->x());
  EXPECT_EQ(toolbar->bounds().bottom(), contents_split->y());
  EXPECT_EQ(0, contents_web_view->x());
  EXPECT_EQ(0, contents_web_view->y());

  // Verify bookmark bar visibility.
  BookmarkBarView* bookmark_bar = browser_view()->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());

  // Bookmark bar is reparented to BrowserView on NTP.
  NavigateAndCommitActiveTabWithTitle(browser,
                                      GURL(chrome::kChromeUINewTabURL),
                                      string16());
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_TRUE(bookmark_bar->IsDetached());
  EXPECT_EQ(browser_view(), bookmark_bar->parent());
  // Find bar host is still at the front of the view hierarchy, followed by
  // the top container.
  EXPECT_EQ(browser_view()->child_count() - 1,
            browser_view()->GetIndexOf(browser_view()->find_bar_host_view()));
  EXPECT_EQ(browser_view()->child_count() - 2,
            browser_view()->GetIndexOf(top_container));

  // Bookmark bar layout on NTP.
  EXPECT_EQ(0, bookmark_bar->x());
  EXPECT_EQ(
      tabstrip->bounds().bottom() +
          toolbar->height() -
          BrowserViewLayout::kToolbarTabStripVerticalOverlap -
          views::NonClientFrameView::kClientEdgeThickness,
      bookmark_bar->y());
  EXPECT_EQ(toolbar->bounds().bottom(), contents_split->y());
  // Contents view has a "top margin" pushing it below the bookmark bar.
  EXPECT_EQ(bookmark_bar->height() -
                views::NonClientFrameView::kClientEdgeThickness,
            contents_web_view->y());

  // Bookmark bar is parented back to top container on normal page.
  NavigateAndCommitActiveTabWithTitle(browser,
                                      GURL("about:blank"),
                                      string16());
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  EXPECT_EQ(top_container, bookmark_bar->parent());
  // Top container is still second from front.
  EXPECT_EQ(browser_view()->child_count() - 2,
            browser_view()->GetIndexOf(top_container));

  BookmarkBarView::DisableAnimationsForTesting(false);
}

#if defined(OS_WIN) && !defined(USE_AURA)

// This class provides functionality to test the incognito window/normal window
// switcher button which is added to Windows 8 metro Chrome.
// We create the BrowserView ourselves in the
// BrowserWithTestWindowTest::CreateBrowserWindow function override and add the
// switcher button to the view. We also provide an incognito profile to ensure
// that the switcher button is visible.
class BrowserViewIncognitoSwitcherTest : public TestWithBrowserView {
 public:
  // Subclass of BrowserView, which overrides the GetRestoreBounds/IsMaximized
  // functions to return dummy values. This is needed because we create the
  // BrowserView instance ourselves and initialize it with the created Browser
  // instance. These functions get called before the underlying Widget is
  // initialized which causes a crash while dereferencing a null native_widget_
  // pointer in the Widget class.
  class TestBrowserView : public BrowserView {
   public:
    virtual ~TestBrowserView() {}

    virtual gfx::Rect GetRestoredBounds() const OVERRIDE {
      return gfx::Rect();
    }
    virtual bool IsMaximized() const OVERRIDE {
      return false;
    }
  };

  BrowserViewIncognitoSwitcherTest()
      : browser_view_(NULL) {}

  virtual void SetUp() OVERRIDE {
    TestWithBrowserView::SetUp();
    browser_view_->Init(browser());
    (new BrowserFrame(browser_view_))->InitBrowserFrame();
    browser_view_->SetBounds(gfx::Rect(10, 10, 500, 500));
    browser_view_->Show();
  }

  virtual void TearDown() OVERRIDE {
    // ok to release the window_ pointer because BrowserViewTest::TearDown
    // deletes the BrowserView instance created.
    release_browser_window();
    BrowserViewTest::TearDown();
    browser_view_ = NULL;
  }

  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE {
    // We need an incognito profile for the window switcher button to be
    // visible.
    // This profile instance is owned by the TestingProfile instance within the
    // BrowserWithTestWindowTest class.
    TestingProfile::Builder builder;
    builder.SetIncognito();
    GetProfile()->SetOffTheRecordProfile(builder.Build());

    browser_view_ = new TestBrowserView();
    browser_view_->SetWindowSwitcherButton(
        MakeWindowSwitcherButton(NULL, false));
    return browser_view_;
  }

 private:
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewIncognitoSwitcherTest);
};

// Test whether the windows incognito/normal browser window switcher button
// is the event handler for a point within its bounds. The event handler for
// a point in the View class is dependent on the order in which children are
// added to it. This test ensures that we don't regress in the window switcher
// functionality when additional children are added to the BrowserView class.
TEST_F(BrowserViewIncognitoSwitcherTest,
       BrowserViewIncognitoSwitcherEventHandlerTest) {
  // |browser_view_| owns the Browser, not the test class.
  EXPECT_FALSE(browser());
  EXPECT_TRUE(browser_view()->browser());
  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  // Validate whether the window switcher button is the target for the position
  // passed in.
  gfx::Point switcher_point(browser_view()->window_switcher_button()->x() + 2,
                            browser_view()->window_switcher_button()->y());
  EXPECT_EQ(browser_view()->GetEventHandlerForPoint(switcher_point),
            browser_view()->window_switcher_button());
}
#endif

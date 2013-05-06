// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/views/controls/single_split_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#endif

using views::FocusManager;

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

// Returns the bounds of |view| in widget coordinates.
gfx::Rect GetRectInWidget(views::View* view) {
  return view->ConvertRectToWidget(view->GetLocalBounds());
}

}

typedef InProcessBrowserTest BrowserViewTest;

IN_PROC_BROWSER_TEST_F(BrowserViewTest, BrowserView) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TopContainerView* top_container = browser_view->top_container();
  TabStrip* tabstrip = browser_view->tabstrip();
  ToolbarView* toolbar = browser_view->toolbar();
  views::SingleSplitView* contents_split =
      browser_view->GetContentsSplitForTest();
  views::WebView* contents_web_view =
      browser_view->GetContentsWebViewForTest();

  // Verify the view hierarchy.
  EXPECT_EQ(top_container, browser_view->tabstrip()->parent());
  EXPECT_EQ(top_container, browser_view->toolbar()->parent());
  EXPECT_EQ(top_container, browser_view->GetBookmarkBarView()->parent());
  EXPECT_EQ(browser_view, browser_view->infobar_container()->parent());

  // Top container is at the front of the view hierarchy.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

  // Verify basic layout.
  EXPECT_EQ(0, top_container->x());
  EXPECT_EQ(0, top_container->y());
  EXPECT_EQ(browser_view->width(), top_container->width());
  // Tabstrip layout varies based on window frame sizes.
  gfx::Point expected_tabstrip_origin = ExpectedTabStripOrigin(browser_view);
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
  BookmarkBarView* bookmark_bar = browser_view->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());

  // Bookmark bar is reparented to BrowserView on NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_TRUE(bookmark_bar->IsDetached());
  EXPECT_EQ(browser_view, bookmark_bar->parent());
  // Top container is still in front.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

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
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  EXPECT_EQ(top_container, bookmark_bar->parent());
  // Top container is still in front.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

  BookmarkBarView::DisableAnimationsForTesting(false);
}

// Active window and focus testing is not reliable on Windows crbug.com/79493
// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
#define MAYBE_FullscreenClearsFocus DISABLED_FullscreenClearsFocus
#else
#define MAYBE_FullscreenClearsFocus FullscreenClearsFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}

//////////////////////////////////////////////////////////////////////////////

// Immersive fullscreen is currently enabled only on Chrome OS.
#if defined(OS_CHROMEOS)

class BrowserViewImmersiveInstantExtendedTest : public InProcessBrowserTest,
                                                public InstantTestBase {
 public:
  BrowserViewImmersiveInstantExtendedTest() {}
  virtual ~BrowserViewImmersiveInstantExtendedTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableImmersiveFullscreenForTest();
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserViewImmersiveInstantExtendedTest);
};

IN_PROC_BROWSER_TEST_F(BrowserViewImmersiveInstantExtendedTest,
                       ImmersiveInstantExtended) {
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  BookmarkBarView::DisableAnimationsForTesting(true);

  // Cache some pointers we'll need below.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ToolbarView* toolbar = browser_view->toolbar();

  // Start up both instant and immersive fullscreen.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());

  ////////////////////////////////////////////////////////////////////////////
  // Test suggestions on a normal web page, which are in an overlay.

  // Focus omnibox, which constructs an overlay web contents.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_TRUE(instant()->model()->mode().is_default());
  // The above testing code doesn't trigger the views location bar focus path,
  // so force it to happen explicitly.
  browser_view->SetFocusToLocationBar(false);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  // Content area is immediately below the tab indicators.
  views::WebView* contents_web_view =
      browser_view->GetContentsWebViewForTest();
  EXPECT_EQ(GetRectInWidget(browser_view).y() + Tab::GetImmersiveHeight(),
            GetRectInWidget(contents_web_view).y());

  // Typing in the omnibox should show suggestions in an overlay view.
  SetOmniboxTextAndWaitForOverlayToShow("santa");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  // Content area is still immediately below the tab indicators.
  EXPECT_EQ(GetRectInWidget(browser_view).y() + Tab::GetImmersiveHeight(),
            GetRectInWidget(contents_web_view).y());
  // Overlay web view (with suggestions) aligns with the bottom of the omnibox.
  gfx::Rect overlay_rect_in_widget = GetRectInWidget(
      browser_view->GetContentsContainerForTest()->GetOverlayWebViewForTest());
  EXPECT_EQ(GetRectInWidget(toolbar).bottom(), overlay_rect_in_widget.y());

  ////////////////////////////////////////////////////////////////////////////
  // Test suggestions on the NTP, which are not in an overlay.

  // Navigate to the Instant NTP, and wait for it to be recognized.
  content::WindowedNotificationObserver instant_tab_observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(browser(),
                                              GURL(chrome::kChromeUINewTabURL),
                                              CURRENT_TAB,
                                              ui_test_utils::BROWSER_TEST_NONE);
  instant_tab_observer.Wait();

  // Type in the omnibox, which should generate suggestions in the main page
  // contents.
  SetOmniboxText("claus");
  // The autocomplete controller isn't done until all the providers are done.
  // Though we don't care about the SearchProvider when we send autocomplete
  // results to the page, we do need to cause it to be "done" to make this test
  // work. Setting the suggestion calls SearchProvider::FinalizeInstantQuery(),
  // thus causing it to be done.
  omnibox()->model()->SetInstantSuggestion(
      InstantSuggestion(ASCIIToUTF16("query"),
                        INSTANT_COMPLETE_NOW,
                        INSTANT_SUGGESTION_SEARCH,
                        ASCIIToUTF16("query")));
  while (!omnibox()->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver autocomplete_observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    autocomplete_observer.Wait();
  }
  // Ensure JavaScript has finished running by executing a blank script.
  EXPECT_TRUE(ExecuteScript(std::string()));
  // We're still revealed, since focus is in the omnibox.
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  // The active web contents are aligned with the toolbar.
  gfx::Rect web_view_rect_in_widget = GetRectInWidget(
      browser_view->GetContentsContainerForTest()->GetActiveWebViewForTest());
  EXPECT_EQ(GetRectInWidget(toolbar).bottom(), web_view_rect_in_widget.y());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

#endif  // defined(OS_CHROMEOS)

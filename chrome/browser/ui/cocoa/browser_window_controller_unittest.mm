// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#include "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using ::testing::Return;

@interface BrowserWindowController (JustForTesting)
// Already defined in BWC.
- (void)saveWindowPositionIfNeeded;
- (void)layoutSubviews;
@end

@interface BrowserWindowController (ExposedForTesting)
// Implementations are below.
- (NSView*)infoBarContainerView;
- (NSView*)toolbarView;
- (NSView*)bookmarkView;
- (BOOL)bookmarkBarVisible;
@end

@implementation BrowserWindowController (ExposedForTesting)
- (NSView*)infoBarContainerView {
  return [infoBarContainerController_ view];
}

- (NSView*)toolbarView {
  return [toolbarController_ view];
}

- (NSView*)bookmarkView {
  return [bookmarkBarController_ view];
}

- (NSView*)findBarView {
  return [findBarCocoaController_ view];
}

- (BOOL)bookmarkBarVisible {
  return [bookmarkBarController_ isVisible];
}
@end

class BrowserWindowControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ = [[BrowserWindowController alloc] initWithBrowser:browser()
                                                     takeOwnership:NO];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

 public:
  BrowserWindowController* controller_;
};

TEST_F(BrowserWindowControllerTest, TestSaveWindowPosition) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  const base::DictionaryValue* browser_window_placement =
      prefs->GetDictionary(prefs::kBrowserWindowPlacement);
  ASSERT_TRUE(browser_window_placement);
  EXPECT_TRUE(browser_window_placement->empty());

  // Ask the window to save its position, then check that a preference
  // exists.
  BrowserList::SetLastActive(browser());
  [controller_ saveWindowPositionIfNeeded];
  browser_window_placement =
      prefs->GetDictionary(prefs::kBrowserWindowPlacement);
  ASSERT_TRUE(browser_window_placement);
  EXPECT_FALSE(browser_window_placement->empty());
}

TEST_F(BrowserWindowControllerTest, TestFullScreenWindow) {
  // Confirm that |-createFullscreenWindow| doesn't return nil.
  // See BrowserWindowFullScreenControllerTest for more fullscreen tests.
  EXPECT_TRUE([controller_ createFullscreenWindow]);
}

TEST_F(BrowserWindowControllerTest, TestNormal) {
  // Force the bookmark bar to be shown.
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
  [controller_ browserWindow]->BookmarkBarStateChanged(
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);

  // Make sure a normal BrowserWindowController is, uh, normal.
  EXPECT_TRUE([controller_ isTabbedWindow]);
  EXPECT_TRUE([controller_ hasTabStrip]);
  EXPECT_FALSE([controller_ hasTitleBar]);
  EXPECT_TRUE([controller_ isBookmarkBarVisible]);

  // And make sure a controller for a pop-up window is not normal.
  // popup_browser will be owned by its window.
  Browser* popup_browser(new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, profile(),
                            chrome::GetActiveDesktop())));
  NSWindow *cocoaWindow = popup_browser->window()->GetNativeWindow();
  BrowserWindowController* controller =
      static_cast<BrowserWindowController*>([cocoaWindow windowController]);
  ASSERT_TRUE([controller isKindOfClass:[BrowserWindowController class]]);
  EXPECT_FALSE([controller isTabbedWindow]);
  EXPECT_FALSE([controller hasTabStrip]);
  EXPECT_TRUE([controller hasTitleBar]);
  EXPECT_FALSE([controller isBookmarkBarVisible]);
  [controller close];
}

TEST_F(BrowserWindowControllerTest, TestSetBounds) {
  // Create a normal browser with bounds smaller than the minimum.
  Browser::CreateParams params(Browser::TYPE_TABBED, profile(),
                               chrome::GetActiveDesktop());
  params.initial_bounds = gfx::Rect(0, 0, 50, 50);
  Browser* browser = new Browser(params);
  NSWindow *cocoaWindow = browser->window()->GetNativeWindow();
  BrowserWindowController* controller =
    static_cast<BrowserWindowController*>([cocoaWindow windowController]);

  ASSERT_TRUE([controller isTabbedWindow]);
  BrowserWindow* browser_window = [controller browserWindow];
  EXPECT_EQ(browser_window, browser->window());
  gfx::Rect bounds = browser_window->GetBounds();
  EXPECT_EQ(400, bounds.width());
  EXPECT_EQ(272, bounds.height());

  // Try to set the bounds smaller than the minimum.
  browser_window->SetBounds(gfx::Rect(0, 0, 50, 50));
  bounds = browser_window->GetBounds();
  EXPECT_EQ(400, bounds.width());
  EXPECT_EQ(272, bounds.height());

  [controller close];
}

TEST_F(BrowserWindowControllerTest, TestSetBoundsPopup) {
  // Create a popup with bounds smaller than the minimum.
  Browser::CreateParams params(Browser::TYPE_POPUP, profile(),
                               chrome::GetActiveDesktop());
  params.initial_bounds = gfx::Rect(0, 0, 50, 50);
  Browser* browser = new Browser(params);
  NSWindow *cocoaWindow = browser->window()->GetNativeWindow();
  BrowserWindowController* controller =
    static_cast<BrowserWindowController*>([cocoaWindow windowController]);

  ASSERT_FALSE([controller isTabbedWindow]);
  BrowserWindow* browser_window = [controller browserWindow];
  EXPECT_EQ(browser_window, browser->window());
  gfx::Rect bounds = browser_window->GetBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(122, bounds.height());

  // Try to set the bounds smaller than the minimum.
  browser_window->SetBounds(gfx::Rect(0, 0, 50, 50));
  bounds = browser_window->GetBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(122, bounds.height());

  [controller close];
}

TEST_F(BrowserWindowControllerTest, TestTheme) {
  [controller_ userChangedTheme];
}

TEST_F(BrowserWindowControllerTest, BookmarkBarControllerIndirection) {
  EXPECT_FALSE([controller_ isBookmarkBarVisible]);

  // Explicitly show the bar. Can't use chrome::ToggleBookmarkBarWhenVisible()
  // because of the notification issues.
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);

  [controller_ browserWindow]->BookmarkBarStateChanged(
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  EXPECT_TRUE([controller_ isBookmarkBarVisible]);
}

#if 0
// TODO(jrg): This crashes trying to create the BookmarkBarController, adding
// an observer to the BookmarkModel.
TEST_F(BrowserWindowControllerTest, TestIncognitoWidthSpace) {
  scoped_ptr<TestingProfile> incognito_profile(new TestingProfile());
  incognito_profile->set_off_the_record(true);
  scoped_ptr<Browser> browser(
      new Browser(Browser::CreateParams(incognito_profile.get(),
                                        chrome::GetActiveDesktop()));
  controller_.reset([[BrowserWindowController alloc]
                              initWithBrowser:browser.get()
                                takeOwnership:NO]);

  NSRect tabFrame = [[controller_ tabStripView] frame];
  [controller_ installIncognitoBadge];
  NSRect newTabFrame = [[controller_ tabStripView] frame];
  EXPECT_GT(tabFrame.size.width, newTabFrame.size.width);

  controller_.release();
}
#endif

namespace {

// Verifies that the toolbar, infobar, tab content area, and download shelf
// completely fill the area under the tabstrip.
void CheckViewPositions(BrowserWindowController* controller) {
  NSRect contentView = [[[controller window] contentView] bounds];
  NSRect tabstrip = [[controller tabStripView] frame];
  NSRect toolbar = [[controller toolbarView] frame];
  NSRect infobar = [[controller infoBarContainerView] frame];
  NSRect contentArea = [[controller tabContentArea] frame];
  NSRect download = [[[controller downloadShelf] view] frame];

  EXPECT_EQ(NSMinY(contentView), NSMinY(download));
  EXPECT_EQ(NSMaxY(download), NSMinY(contentArea));
  EXPECT_EQ(NSMaxY(contentArea), NSMinY(infobar));

  // Bookmark bar frame is random memory when hidden.
  if ([controller bookmarkBarVisible]) {
    NSRect bookmark = [[controller bookmarkView] frame];
    EXPECT_EQ(NSMaxY(infobar), NSMinY(bookmark));
    EXPECT_EQ(NSMaxY(bookmark), NSMinY(toolbar));
    EXPECT_FALSE([[controller bookmarkView] isHidden]);
  } else {
    EXPECT_EQ(NSMaxY(infobar), NSMinY(toolbar));
    EXPECT_TRUE([[controller bookmarkView] isHidden]);
  }

  // Toolbar should start immediately under the tabstrip, but the tabstrip is
  // not necessarily fixed with respect to the content view.
  EXPECT_EQ(NSMinY(tabstrip), NSMaxY(toolbar));
}

}  // end namespace

TEST_F(BrowserWindowControllerTest, TestAdjustWindowHeight) {
  NSWindow* window = [controller_ window];
  NSRect workarea = [[window screen] visibleFrame];

  // Place the window well above the bottom of the screen and try to adjust its
  // height. It should change appropriately (and only downwards). Then get it to
  // shrink by the same amount; it should return to its original state.
  NSRect initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y + 100,
                                   200, 200);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  NSRect finalFrame = [window frame];
  EXPECT_FALSE(NSEqualRects(finalFrame, initialFrame));
  EXPECT_FLOAT_EQ(NSMaxY(finalFrame), NSMaxY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) + 40);
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMaxY(finalFrame), NSMaxY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame));

  // Place the window at the bottom of the screen and try again.  Its height
  // should still change, but it should not grow down below the work area; it
  // should instead move upwards. Then shrink it and make sure it goes back to
  // the way it was.
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y, 200, 200);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_FALSE(NSEqualRects(finalFrame, initialFrame));
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) + 40);
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame));

  // Put the window slightly offscreen and try again.  The height should not
  // change this time.
  initialFrame = NSMakeRect(workarea.origin.x - 10, 0, 200, 200);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  EXPECT_TRUE(NSEqualRects([window frame], initialFrame));
  [controller_ adjustWindowHeightBy:-40];
  EXPECT_TRUE(NSEqualRects([window frame], initialFrame));

  // Make the window the same size as the workarea.  Resizing both larger and
  // smaller should have no effect.
  [window setFrame:workarea display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  EXPECT_TRUE(NSEqualRects([window frame], workarea));
  [controller_ adjustWindowHeightBy:-40];
  EXPECT_TRUE(NSEqualRects([window frame], workarea));

  // Make the window smaller than the workarea and place it near the bottom of
  // the workarea.  The window should grow down until it hits the bottom and
  // then continue to grow up. Then shrink it, and it should return to where it
  // was.
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y + 5,
                            200, 200);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(workarea), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) + 40);
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(initialFrame), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(initialFrame), NSHeight(finalFrame));

  // Inset the window slightly from the workarea.  It should not grow to be
  // larger than the workarea. Shrink it; it should return to where it started.
  initialFrame = NSInsetRect(workarea, 0, 5);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(workarea), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(workarea), NSHeight(finalFrame));
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(initialFrame), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(initialFrame), NSHeight(finalFrame));

  // Place the window at the bottom of the screen and grow; it should grow
  // upwards. Move the window off the bottom, then shrink. It should then shrink
  // from the bottom.
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y, 200, 200);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_FALSE(NSEqualRects(finalFrame, initialFrame));
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) + 40);
  NSPoint oldOrigin = initialFrame.origin;
  NSPoint newOrigin = NSMakePoint(oldOrigin.x, oldOrigin.y + 10);
  [window setFrameOrigin:newOrigin];
  initialFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(initialFrame), oldOrigin.y + 10);
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame) + 40);
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) - 40);

  // Do the "inset" test above, but using multiple calls to
  // |-adjustWindowHeightBy|; the result should be the same.
  initialFrame = NSInsetRect(workarea, 0, 5);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  for (int i = 0; i < 8; i++)
    [controller_ adjustWindowHeightBy:5];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(workarea), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(workarea), NSHeight(finalFrame));
  for (int i = 0; i < 8; i++)
    [controller_ adjustWindowHeightBy:-5];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(initialFrame), NSMinY(finalFrame));
  EXPECT_FLOAT_EQ(NSHeight(initialFrame), NSHeight(finalFrame));
}

// Test to make sure resizing and relaying-out subviews works correctly.
TEST_F(BrowserWindowControllerTest, TestResizeViews) {
  TabStripView* tabstrip = [controller_ tabStripView];
  NSView* contentView = [[tabstrip window] contentView];
  NSView* toolbar = [controller_ toolbarView];
  NSView* infobar = [controller_ infoBarContainerView];

  // We need to muck with the views a bit to put us in a consistent state before
  // we start resizing.  In particular, we need to move the tab strip to be
  // immediately above the content area, since we layout views to be directly
  // under the tab strip.
  NSRect tabstripFrame = [tabstrip frame];
  tabstripFrame.origin.y = NSMaxY([contentView frame]);
  [tabstrip setFrame:tabstripFrame];

  // The download shelf is created lazily.  Force-create it and set its initial
  // height to 0.
  NSView* download = [[controller_ downloadShelf] view];
  NSRect downloadFrame = [download frame];
  downloadFrame.size.height = 0;
  [download setFrame:downloadFrame];

  // Force a layout and check each view's frame.
  [controller_ layoutSubviews];
  CheckViewPositions(controller_);

  // Expand the infobar to 60px and recheck
  [controller_ resizeView:infobar newHeight:60];
  CheckViewPositions(controller_);

  // Expand the toolbar to 64px and recheck
  [controller_ resizeView:toolbar newHeight:64];
  CheckViewPositions(controller_);

  // Add a 30px download shelf and recheck
  [controller_ resizeView:download newHeight:30];
  CheckViewPositions(controller_);

  // Shrink the infobar to 0px and toolbar to 39px and recheck
  [controller_ resizeView:infobar newHeight:0];
  [controller_ resizeView:toolbar newHeight:39];
  CheckViewPositions(controller_);
}

TEST_F(BrowserWindowControllerTest, TestResizeViewsWithBookmarkBar) {
  // Force a display of the bookmark bar.
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
  [controller_ browserWindow]->BookmarkBarStateChanged(
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);

  TabStripView* tabstrip = [controller_ tabStripView];
  NSView* contentView = [[tabstrip window] contentView];
  NSView* toolbar = [controller_ toolbarView];
  NSView* bookmark = [controller_ bookmarkView];
  NSView* infobar = [controller_ infoBarContainerView];

  // We need to muck with the views a bit to put us in a consistent state before
  // we start resizing.  In particular, we need to move the tab strip to be
  // immediately above the content area, since we layout views to be directly
  // under the tab strip.
  NSRect tabstripFrame = [tabstrip frame];
  tabstripFrame.origin.y = NSMaxY([contentView frame]);
  [tabstrip setFrame:tabstripFrame];

  // The download shelf is created lazily.  Force-create it and set its initial
  // height to 0.
  NSView* download = [[controller_ downloadShelf] view];
  NSRect downloadFrame = [download frame];
  downloadFrame.size.height = 0;
  [download setFrame:downloadFrame];

  // Force a layout and check each view's frame.
  [controller_ layoutSubviews];
  CheckViewPositions(controller_);

  // Add the bookmark bar and recheck.
  [controller_ resizeView:bookmark newHeight:40];
  CheckViewPositions(controller_);

  // Expand the infobar to 60px and recheck
  [controller_ resizeView:infobar newHeight:60];
  CheckViewPositions(controller_);

  // Expand the toolbar to 64px and recheck
  [controller_ resizeView:toolbar newHeight:64];
  CheckViewPositions(controller_);

  // Add a 30px download shelf and recheck
  [controller_ resizeView:download newHeight:30];
  CheckViewPositions(controller_);

  // Remove the bookmark bar and recheck
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, false);
  [controller_ resizeView:bookmark newHeight:0];
  CheckViewPositions(controller_);

  // Shrink the infobar to 0px and toolbar to 39px and recheck
  [controller_ resizeView:infobar newHeight:0];
  [controller_ resizeView:toolbar newHeight:39];
  CheckViewPositions(controller_);
}

// Make sure, by default, the bookmark bar and the toolbar are the same width.
TEST_F(BrowserWindowControllerTest, BookmarkBarIsSameWidth) {
  // Set the pref to the bookmark bar is visible when the toolbar is
  // first created.
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);

  // Make sure the bookmark bar is the same width as the toolbar
  NSView* bookmarkBarView = [controller_ bookmarkView];
  NSView* toolbarView = [controller_ toolbarView];
  EXPECT_EQ([toolbarView frame].size.width,
            [bookmarkBarView frame].size.width);
}

TEST_F(BrowserWindowControllerTest, TestTopRightForBubble) {
  // The bookmark bubble must be attached to a lit and visible star.
  [controller_ setStarredState:YES];
  NSPoint p = [controller_ bookmarkBubblePoint];
  NSRect all = [[controller_ window] frame];

  // As a sanity check make sure the point is vaguely in the top right
  // of the window.
  EXPECT_GT(p.y, all.origin.y + (all.size.height/2));
  EXPECT_GT(p.x, all.origin.x + (all.size.width/2));
}

// By the "zoom frame", we mean what Apple calls the "standard frame".
TEST_F(BrowserWindowControllerTest, TestZoomFrame) {
  NSWindow* window = [controller_ window];
  ASSERT_TRUE(window);
  NSRect screenFrame = [[window screen] visibleFrame];
  ASSERT_FALSE(NSIsEmptyRect(screenFrame));

  // Minimum zoomed width is the larger of 60% of available horizontal space or
  // 60% of available vertical space, subject to available horizontal space.
  CGFloat minZoomWidth =
      std::min(std::max((CGFloat)0.6 * screenFrame.size.width,
                        (CGFloat)0.6 * screenFrame.size.height),
               screenFrame.size.width);

  // |testFrame| is the size of the window we start out with, and |zoomFrame| is
  // the one returned by |-windowWillUseStandardFrame:defaultFrame:|.
  NSRect testFrame;
  NSRect zoomFrame;

  // 1. Test a case where it zooms the window both horizontally and vertically,
  // and only moves it vertically. "+ 32", etc. are just arbitrary constants
  // used to check that the window is moved properly and not just to the origin;
  // they should be small enough to not shove windows off the screen.
  testFrame.size.width = 0.5 * minZoomWidth;
  testFrame.size.height = 0.5 * screenFrame.size.height;
  testFrame.origin.x = screenFrame.origin.x + 32;  // See above.
  testFrame.origin.y = screenFrame.origin.y + 23;
  [window setFrame:testFrame display:NO];
  zoomFrame = [controller_ windowWillUseStandardFrame:window
                                         defaultFrame:screenFrame];
  EXPECT_LE(minZoomWidth, zoomFrame.size.width);
  EXPECT_EQ(screenFrame.size.height, zoomFrame.size.height);
  EXPECT_EQ(testFrame.origin.x, zoomFrame.origin.x);
  EXPECT_EQ(screenFrame.origin.y, zoomFrame.origin.y);

  // 2. Test a case where it zooms the window only horizontally, and only moves
  // it horizontally.
  testFrame.size.width = 0.5 * minZoomWidth;
  testFrame.size.height = screenFrame.size.height;
  testFrame.origin.x = screenFrame.origin.x + screenFrame.size.width -
                       testFrame.size.width;
  testFrame.origin.y = screenFrame.origin.y;
  [window setFrame:testFrame display:NO];
  zoomFrame = [controller_ windowWillUseStandardFrame:window
                                         defaultFrame:screenFrame];
  EXPECT_LE(minZoomWidth, zoomFrame.size.width);
  EXPECT_EQ(screenFrame.size.height, zoomFrame.size.height);
  EXPECT_EQ(screenFrame.origin.x + screenFrame.size.width -
            zoomFrame.size.width, zoomFrame.origin.x);
  EXPECT_EQ(screenFrame.origin.y, zoomFrame.origin.y);

  // 3. Test a case where it zooms the window only vertically, and only moves it
  // vertically.
  testFrame.size.width = std::min((CGFloat)1.1 * minZoomWidth,
                                  screenFrame.size.width);
  testFrame.size.height = 0.3 * screenFrame.size.height;
  testFrame.origin.x = screenFrame.origin.x + 32;  // See above (in 1.).
  testFrame.origin.y = screenFrame.origin.y + 123;
  [window setFrame:testFrame display:NO];
  zoomFrame = [controller_ windowWillUseStandardFrame:window
                                         defaultFrame:screenFrame];
  // Use the actual width of the window frame, since it's subject to rounding.
  EXPECT_EQ([window frame].size.width, zoomFrame.size.width);
  EXPECT_EQ(screenFrame.size.height, zoomFrame.size.height);
  EXPECT_EQ(testFrame.origin.x, zoomFrame.origin.x);
  EXPECT_EQ(screenFrame.origin.y, zoomFrame.origin.y);

  // 4. Test a case where zooming should do nothing (i.e., we're already at a
  // zoomed frame).
  testFrame.size.width = std::min((CGFloat)1.1 * minZoomWidth,
                                  screenFrame.size.width);
  testFrame.size.height = screenFrame.size.height;
  testFrame.origin.x = screenFrame.origin.x + 32;  // See above (in 1.).
  testFrame.origin.y = screenFrame.origin.y;
  [window setFrame:testFrame display:NO];
  zoomFrame = [controller_ windowWillUseStandardFrame:window
                                         defaultFrame:screenFrame];
  // Use the actual width of the window frame, since it's subject to rounding.
  EXPECT_EQ([window frame].size.width, zoomFrame.size.width);
  EXPECT_EQ(screenFrame.size.height, zoomFrame.size.height);
  EXPECT_EQ(testFrame.origin.x, zoomFrame.origin.x);
  EXPECT_EQ(screenFrame.origin.y, zoomFrame.origin.y);
}

TEST_F(BrowserWindowControllerTest, TestFindBarOnTop) {
  FindBarBridge bridge(NULL);
  [controller_ addFindBar:bridge.find_bar_cocoa_controller()];

  // Test that the Z-order of the find bar is on top of everything.
  NSArray* subviews = [controller_.chromeContentView subviews];
  NSUInteger findBar_index =
      [subviews indexOfObject:[controller_ findBarView]];
  EXPECT_NE(NSNotFound, findBar_index);
  NSUInteger toolbar_index =
      [subviews indexOfObject:[controller_ toolbarView]];
  EXPECT_NE(NSNotFound, toolbar_index);
  NSUInteger bookmark_index =
      [subviews indexOfObject:[controller_ bookmarkView]];
  EXPECT_NE(NSNotFound, bookmark_index);

  EXPECT_GT(findBar_index, toolbar_index);
  EXPECT_GT(findBar_index, bookmark_index);
}

TEST_F(BrowserWindowControllerTest, TestSigninMenuItemNoErrors) {
  base::scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];

  NSString* startSignin =
    l10n_util::GetNSStringFWithFixup(
        IDS_SYNC_MENU_PRE_SYNCED_LABEL,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  // Make sure shouldShow parameter is obeyed, and we get the default
  // label if not signed in.
  [BrowserWindowController updateSigninItem:syncMenuItem
                                 shouldShow:YES
                             currentProfile:profile()];

  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSignin]);
  EXPECT_FALSE([syncMenuItem isHidden]);

  [BrowserWindowController updateSigninItem:syncMenuItem
                                 shouldShow:NO
                             currentProfile:profile()];
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSignin]);
  EXPECT_TRUE([syncMenuItem isHidden]);

  // Now sign in.
  std::string username = "foo@example.com";
  NSString* alreadySignedIn =
    l10n_util::GetNSStringFWithFixup(IDS_SYNC_MENU_SYNCED_LABEL,
                                     base::UTF8ToUTF16(username));
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile());
  signin->SetAuthenticatedUsername(username);
  ProfileSyncService* sync =
    ProfileSyncServiceFactory::GetForProfile(profile());
  sync->SetSyncSetupCompleted();
  [BrowserWindowController updateSigninItem:syncMenuItem
                                 shouldShow:YES
                             currentProfile:profile()];
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:alreadySignedIn]);
  EXPECT_FALSE([syncMenuItem isHidden]);
}

TEST_F(BrowserWindowControllerTest, TestSigninMenuItemAuthError) {
  base::scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];

  // Now sign in.
  std::string username = "foo@example.com";
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile());
  signin->SetAuthenticatedUsername(username);
  ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetForProfile(profile());
  sync->SetSyncSetupCompleted();
  // Force an auth error.
  FakeAuthStatusProvider provider(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile())->
          signin_error_controller());
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  provider.SetAuthError("user@gmail.com", "user@gmail.com", error);
  [BrowserWindowController updateSigninItem:syncMenuItem
                                 shouldShow:YES
                             currentProfile:profile()];
  NSString* authError =
    l10n_util::GetNSStringWithFixup(IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:authError]);
  EXPECT_FALSE([syncMenuItem isHidden]);

}

// If there's a separator after the signin menu item, make sure it is hidden/
// shown when the signin menu item is.
TEST_F(BrowserWindowControllerTest, TestSigninMenuItemWithSeparator) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* signinMenuItem =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [signinMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingSeparator = [NSMenuItem separatorItem];
  [menu addItem:followingSeparator];
  [signinMenuItem setHidden:NO];
  [followingSeparator setHidden:NO];

  [BrowserWindowController updateSigninItem:signinMenuItem
                                 shouldShow:NO
                             currentProfile:profile()];

  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_TRUE([signinMenuItem isHidden]);
  EXPECT_TRUE([followingSeparator isHidden]);

  [BrowserWindowController updateSigninItem:signinMenuItem
                                 shouldShow:YES
                             currentProfile:profile()];

  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_FALSE([signinMenuItem isHidden]);
  EXPECT_FALSE([followingSeparator isHidden]);
}

// If there's a non-separator item after the signin menu item, it should not
// change state when the signin menu item is hidden/shown.
TEST_F(BrowserWindowControllerTest, TestSigninMenuItemWithNonSeparator) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* signinMenuItem =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [signinMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingNonSeparator =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [signinMenuItem setHidden:NO];
  [followingNonSeparator setHidden:NO];

  [BrowserWindowController updateSigninItem:signinMenuItem
                                 shouldShow:NO
                             currentProfile:profile()];

  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_TRUE([signinMenuItem isHidden]);
  EXPECT_FALSE([followingNonSeparator isHidden]);

  [followingNonSeparator setHidden:YES];
  [BrowserWindowController updateSigninItem:signinMenuItem
                                 shouldShow:YES
                             currentProfile:profile()];

  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([signinMenuItem isHidden]);
  EXPECT_TRUE([followingNonSeparator isHidden]);
}

// Verify that hit testing works correctly when the bookmark bar overlaps
// web contents.
TEST_F(BrowserWindowControllerTest, BookmarkBarHitTest) {
  profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
  [controller_ browserWindow]->BookmarkBarStateChanged(
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);

  NSView* bookmarkView = [controller_ bookmarkView];
  NSView* contentView = [[controller_ window] contentView];
  NSPoint point = [bookmarkView convertPoint:NSMakePoint(1, 1)
                                      toView:[contentView superview]];

  EXPECT_TRUE([[contentView hitTest:point] isDescendantOf:bookmarkView]);
}

@interface BrowserWindowControllerFakeFullscreen : BrowserWindowController {
 @private
  // We release the window ourselves, so we don't have to rely on the unittest
  // doing it for us.
  base::scoped_nsobject<NSWindow> testFullscreenWindow_;
}
@end

class BrowserWindowFullScreenControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ =
        [[BrowserWindowControllerFakeFullscreen alloc] initWithBrowser:browser()
                                                         takeOwnership:NO];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

 public:
  BrowserWindowController* controller_;
};

// Check if the window is front most or if one of its child windows (such
// as a status bubble) is front most.
static bool IsFrontWindow(NSWindow *window) {
  NSWindow* frontmostWindow = [[NSApp orderedWindows] objectAtIndex:0];
  return [frontmostWindow isEqual:window] ||
         [[frontmostWindow parentWindow] isEqual:window];
}

void WaitForFullScreenTransition() {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::NotificationService::AllSources());
  observer.Wait();
}

TEST_F(BrowserWindowFullScreenControllerTest, TestFullscreen) {
  [controller_ showWindow:nil];
  EXPECT_FALSE([controller_ isFullscreen]);

  [controller_ enterFullscreen];
  WaitForFullScreenTransition();
  EXPECT_TRUE([controller_ isFullscreen]);

  [controller_ exitFullscreen];
  WaitForFullScreenTransition();
  EXPECT_FALSE([controller_ isFullscreen]);
}

// If this test fails, it is usually a sign that the bots have some sort of
// problem (such as a modal dialog up).  This tests is a very useful canary, so
// please do not mark it as flaky without first verifying that there are no bot
// problems.
TEST_F(BrowserWindowFullScreenControllerTest, TestActivate) {
  [controller_ showWindow:nil];

  EXPECT_FALSE([controller_ isFullscreen]);

  [controller_ activate];
  EXPECT_TRUE(IsFrontWindow([controller_ window]));

  [controller_ enterFullscreen];
  WaitForFullScreenTransition();
  [controller_ activate];

  // No fullscreen window on 10.7+.
  if (base::mac::IsOSSnowLeopard())
    EXPECT_TRUE(IsFrontWindow([controller_ createFullscreenWindow]));

  // We have to cleanup after ourselves by unfullscreening.
  [controller_ exitFullscreen];
  WaitForFullScreenTransition();
}

@implementation BrowserWindowControllerFakeFullscreen
// Override |-createFullscreenWindow| to return a dummy window. This isn't
// needed to pass the test, but because the dummy window is only 100x100, it
// prevents the real fullscreen window from flashing up and taking over the
// whole screen. We have to return an actual window because |-layoutSubviews|
// looks at the window's frame.
- (NSWindow*)createFullscreenWindow {
  if (testFullscreenWindow_.get())
    return testFullscreenWindow_.get();

  testFullscreenWindow_.reset(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,400,400)
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [[testFullscreenWindow_ contentView] cr_setWantsLayer:YES];
  return testFullscreenWindow_.get();
}
@end

/* TODO(???): test other methods of BrowserWindowController */

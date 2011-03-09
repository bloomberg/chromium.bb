// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface BrowserWindowController (JustForTesting)
// Already defined in BWC.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;
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

- (NSSplitView*)devToolsView {
  return static_cast<NSSplitView*>([devToolsController_ view]);
}

- (NSView*)sidebarView {
  return [sidebarController_ view];
}

- (BOOL)bookmarkBarVisible {
  return [bookmarkBarController_ isVisible];
}
@end

class BrowserWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    Browser* browser = browser_helper_.browser();
    controller_ = [[BrowserWindowController alloc] initWithBrowser:browser
                                                     takeOwnership:NO];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

 public:
  BrowserTestHelper browser_helper_;
  BrowserWindowController* controller_;
};

TEST_F(BrowserWindowControllerTest, TestSaveWindowPosition) {
  PrefService* prefs = browser_helper_.profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  const DictionaryValue* browser_window_placement =
      prefs->GetDictionary(prefs::kBrowserWindowPlacement);
  ASSERT_TRUE(browser_window_placement);
  EXPECT_TRUE(browser_window_placement->empty());

  // Ask the window to save its position, then check that a preference
  // exists.
  [controller_ saveWindowPositionToPrefs:prefs];
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
  browser_helper_.profile()->GetPrefs()->
      SetBoolean(prefs::kShowBookmarkBar, true);
  [controller_ updateBookmarkBarVisibilityWithAnimation:NO];

  // Make sure a normal BrowserWindowController is, uh, normal.
  EXPECT_TRUE([controller_ isNormalWindow]);
  EXPECT_TRUE([controller_ hasTabStrip]);
  EXPECT_FALSE([controller_ hasTitleBar]);
  EXPECT_TRUE([controller_ isBookmarkBarVisible]);

  // And make sure a controller for a pop-up window is not normal.
  // popup_browser will be owned by its window.
  Browser *popup_browser(Browser::CreateForType(Browser::TYPE_POPUP,
                                                browser_helper_.profile()));
  NSWindow *cocoaWindow = popup_browser->window()->GetNativeHandle();
  BrowserWindowController* controller =
      static_cast<BrowserWindowController*>([cocoaWindow windowController]);
  ASSERT_TRUE([controller isKindOfClass:[BrowserWindowController class]]);
  EXPECT_FALSE([controller isNormalWindow]);
  EXPECT_FALSE([controller hasTabStrip]);
  EXPECT_TRUE([controller hasTitleBar]);
  EXPECT_FALSE([controller isBookmarkBarVisible]);
  [controller close];
}

TEST_F(BrowserWindowControllerTest, TestTheme) {
  [controller_ userChangedTheme];
}

TEST_F(BrowserWindowControllerTest, BookmarkBarControllerIndirection) {
  EXPECT_FALSE([controller_ isBookmarkBarVisible]);

  // Explicitly show the bar. Can't use bookmark_utils::ToggleWhenVisible()
  // because of the notification issues.
  browser_helper_.profile()->GetPrefs()->
      SetBoolean(prefs::kShowBookmarkBar, true);

  [controller_ updateBookmarkBarVisibilityWithAnimation:NO];
  EXPECT_TRUE([controller_ isBookmarkBarVisible]);
}

#if 0
// TODO(jrg): This crashes trying to create the BookmarkBarController, adding
// an observer to the BookmarkModel.
TEST_F(BrowserWindowControllerTest, TestIncognitoWidthSpace) {
  scoped_ptr<TestingProfile> incognito_profile(new TestingProfile());
  incognito_profile->set_off_the_record(true);
  scoped_ptr<Browser> browser(new Browser(Browser::TYPE_NORMAL,
                                          incognito_profile.get()));
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
  browser_helper_.profile()->GetPrefs()->
      SetBoolean(prefs::kShowBookmarkBar, true);
  [controller_ updateBookmarkBarVisibilityWithAnimation:NO];

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
  browser_helper_.profile()->GetPrefs()->
      SetBoolean(prefs::kShowBookmarkBar, false);
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
  browser_helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kShowBookmarkBar, true);

  // Make sure the bookmark bar is the same width as the toolbar
  NSView* bookmarkBarView = [controller_ bookmarkView];
  NSView* toolbarView = [controller_ toolbarView];
  EXPECT_EQ([toolbarView frame].size.width,
            [bookmarkBarView frame].size.width);
}

TEST_F(BrowserWindowControllerTest, TestTopRightForBubble) {
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
  FindBarBridge bridge;
  [controller_ addFindBar:bridge.find_bar_cocoa_controller()];

  // Test that the Z-order of the find bar is on top of everything.
  NSArray* subviews = [[[controller_ window] contentView] subviews];
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

// Tests that the sidebar view and devtools view are both non-opaque.
TEST_F(BrowserWindowControllerTest, TestSplitViewsAreNotOpaque) {
  // Add a subview to the sidebar view to mimic what happens when a tab is added
  // to the window.  NSSplitView only marks itself as non-opaque when one of its
  // subviews is non-opaque, so the test will not pass without this subview.
  scoped_nsobject<NSView> view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)]);
  [[controller_ sidebarView] addSubview:view];

  EXPECT_FALSE([[controller_ tabContentArea] isOpaque]);
  EXPECT_FALSE([[controller_ devToolsView] isOpaque]);
  EXPECT_FALSE([[controller_ sidebarView] isOpaque]);
}

// Tests that status bubble's base frame does move when devTools are docked.
TEST_F(BrowserWindowControllerTest, TestStatusBubblePositioning) {
  ASSERT_EQ(1U, [[[controller_ devToolsView] subviews] count]);

  NSPoint bubbleOrigin = [controller_ statusBubbleBaseFrame].origin;

  // Add a fake subview to devToolsView to emulate docked devTools.
  scoped_nsobject<NSView> view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)]);
  [[controller_ devToolsView] addSubview:view];
  [[controller_ devToolsView] adjustSubviews];

  NSPoint bubbleOriginWithDevTools = [controller_ statusBubbleBaseFrame].origin;

  // Make sure that status bubble frame is moved.
  EXPECT_FALSE(NSEqualPoints(bubbleOrigin, bubbleOriginWithDevTools));
}

@interface BrowserWindowControllerFakeFullscreen : BrowserWindowController {
 @private
  // We release the window ourselves, so we don't have to rely on the unittest
  // doing it for us.
  scoped_nsobject<NSWindow> fullscreenWindow_;
}
@end

class BrowserWindowFullScreenControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    Browser* browser = browser_helper_.browser();
    controller_ =
        [[BrowserWindowControllerFakeFullscreen alloc] initWithBrowser:browser
                                                         takeOwnership:NO];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

 public:
  BrowserTestHelper browser_helper_;
  BrowserWindowController* controller_;
};

@interface BrowserWindowController (PrivateAPI)
- (BOOL)supportsFullscreen;
@end

// Check if the window is front most or if one of its child windows (such
// as a status bubble) is front most.
static bool IsFrontWindow(NSWindow *window) {
  NSWindow* frontmostWindow = [[NSApp orderedWindows] objectAtIndex:0];
  return [frontmostWindow isEqual:window] ||
         [[frontmostWindow parentWindow] isEqual:window];
}

TEST_F(BrowserWindowFullScreenControllerTest, TestFullscreen) {
  EXPECT_FALSE([controller_ isFullscreen]);
  [controller_ setFullscreen:YES];
  EXPECT_TRUE([controller_ isFullscreen]);
  [controller_ setFullscreen:NO];
  EXPECT_FALSE([controller_ isFullscreen]);
}

// If this test fails, it is usually a sign that the bots have some sort of
// problem (such as a modal dialog up).  This tests is a very useful canary, so
// please do not mark it as flaky without first verifying that there are no bot
// problems.
TEST_F(BrowserWindowFullScreenControllerTest, TestActivate) {
  EXPECT_FALSE([controller_ isFullscreen]);

  [controller_ activate];
  EXPECT_TRUE(IsFrontWindow([controller_ window]));

  [controller_ setFullscreen:YES];
  [controller_ activate];
  EXPECT_TRUE(IsFrontWindow([controller_ createFullscreenWindow]));

  // We have to cleanup after ourselves by unfullscreening.
  [controller_ setFullscreen:NO];
}

@implementation BrowserWindowControllerFakeFullscreen
// Override |-createFullscreenWindow| to return a dummy window. This isn't
// needed to pass the test, but because the dummy window is only 100x100, it
// prevents the real fullscreen window from flashing up and taking over the
// whole screen. We have to return an actual window because |-layoutSubviews|
// looks at the window's frame.
- (NSWindow*)createFullscreenWindow {
  if (fullscreenWindow_.get())
    return fullscreenWindow_.get();

  fullscreenWindow_.reset(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,400,400)
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  return fullscreenWindow_.get();
}
@end

/* TODO(???): test other methods of BrowserWindowController */

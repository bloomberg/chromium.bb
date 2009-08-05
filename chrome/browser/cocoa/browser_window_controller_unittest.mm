// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface BrowserWindowController (JustForTesting)
// Already defined in BWC.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;
- (void)layoutSubviews;
@end

@interface BrowserWindowController (ExposedForTesting)
// Implementations are below.
- (NSView*)infoBarContainerView;
- (NSView*)toolbarView;
@end

@implementation BrowserWindowController (ExposedForTesting)
- (NSView*)infoBarContainerView {
  return [infoBarContainerController_ view];
}

- (NSView*)toolbarView {
  return [toolbarController_ view];
}
@end

class BrowserWindowControllerTest : public testing::Test {
  virtual void SetUp() {
    controller_.reset([[BrowserWindowController alloc]
                        initWithBrowser:browser_helper_.browser()
                          takeOwnership:NO]);
  }

 public:
  // Order is very important here.  We want the controller deleted
  // before the pool, and want the pool deleted before
  // BrowserTestHelper.
  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  base::ScopedNSAutoreleasePool pool_;
  scoped_nsobject<BrowserWindowController> controller_;
};

TEST_F(BrowserWindowControllerTest, TestSaveWindowPosition) {
  PrefService* prefs = browser_helper_.profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  ASSERT_TRUE(prefs->GetDictionary(prefs::kBrowserWindowPlacement) == NULL);

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.
  [controller_ saveWindowPositionToPrefs:prefs];
  EXPECT_TRUE(prefs->GetDictionary(prefs::kBrowserWindowPlacement) != NULL);
}

@interface BrowserWindowControllerFakeFullscreen : BrowserWindowController {
}
@end
@implementation BrowserWindowControllerFakeFullscreen
// This isn't needed to pass the test, but does prevent an actual
// fullscreen from happening.
- (NSWindow*)fullscreenWindow {
  return nil;
}
@end

TEST_F(BrowserWindowControllerTest, TestFullscreen) {
  // Note use of "controller", not "controller_"
  scoped_nsobject<BrowserWindowController> controller;
  controller.reset([[BrowserWindowControllerFakeFullscreen alloc]
                        initWithBrowser:browser_helper_.browser()
                          takeOwnership:NO]);
  EXPECT_FALSE([controller isFullscreen]);
  [controller setFullscreen:YES];
  EXPECT_TRUE([controller isFullscreen]);
  [controller setFullscreen:NO];
  EXPECT_FALSE([controller isFullscreen]);

  // Confirm the real fullscreen command doesn't return nil
  EXPECT_TRUE([controller_ fullscreenWindow]);
}

TEST_F(BrowserWindowControllerTest, TestNormal) {
  // Make sure a normal BrowserWindowController is, uh, normal.
  EXPECT_TRUE([controller_ isNormalWindow]);

  // And make sure a controller for a pop-up window is not normal.
  scoped_ptr<Browser> popup_browser(Browser::CreateForPopup(
                                      browser_helper_.profile()));
  controller_.reset([[BrowserWindowController alloc]
                              initWithBrowser:popup_browser.get()
                                takeOwnership:NO]);
  EXPECT_FALSE([controller_ isNormalWindow]);

  // The created BrowserWindowController gets autoreleased, so make
  // sure we don't also release it.
  // (Confirmed with valgrind).
  controller_.release();
}

@interface GTMTheme (BrowserThemeProviderInitialization)
+ (GTMTheme *)themeWithBrowserThemeProvider:(BrowserThemeProvider *)provider
                             isOffTheRecord:(BOOL)isOffTheRecord;
@end

TEST_F(BrowserWindowControllerTest, TestTheme) {
  [controller_ userChangedTheme];
}

TEST_F(BrowserWindowControllerTest, BookmarkBarControllerIndirection) {
  EXPECT_FALSE([controller_ isBookmarkBarVisible]);
  [controller_ toggleBookmarkBar];
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

@interface BrowserWindowControllerResizePong : BrowserWindowController {
}
@end

@implementation BrowserWindowControllerResizePong
@end

// Test to make sure resizing and relaying-out subviews works correctly.
TEST_F(BrowserWindowControllerTest, TestResizeViews) {
  TabStripView* tabstrip = [controller_ tabStripView];
  NSView* contentView = [[tabstrip window] contentView];
  NSView* toolbar = [controller_ toolbarView];
  NSView* infobar = [controller_ infoBarContainerView];
  NSView* contentArea = [controller_ tabContentArea];

  // We need to muck with the views a bit to put us in a consistent state before
  // we start resizing.  In particular, we need to move the tab strip to be
  // immediately above the content area, since we layout views to be directly
  // under the tab strip.  We also explicitly set the contentView's frame to be
  // 800x600.
  [contentView setFrame:NSMakeRect(0, 0, 800, 600)];
  NSRect tabstripFrame = [tabstrip frame];
  tabstripFrame.origin.y = NSMaxY([contentView frame]);
  [tabstrip setFrame:tabstripFrame];

  // Make sure each view is as tall as we expect.
  ASSERT_EQ(39, NSHeight([toolbar frame]));
  ASSERT_EQ(0, NSHeight([infobar frame]));

  // Force a layout and check each view's frame.
  // contentView should be at 0,0 800x600
  // contentArea should be at 0,0 800x561
  // infobar should be at 0,561 800x0
  // toolbar should be at 0,561 800x39
  [controller_ layoutSubviews];
  EXPECT_TRUE(NSEqualRects([contentView frame], NSMakeRect(0, 0, 800, 600)));
  EXPECT_TRUE(NSEqualRects([contentArea frame], NSMakeRect(0, 0, 800, 561)));
  EXPECT_TRUE(NSEqualRects([infobar frame], NSMakeRect(0, 561, 800, 0)));
  EXPECT_TRUE(NSEqualRects([toolbar frame], NSMakeRect(0, 561, 800, 39)));

  // Expand the infobar to 60px and recheck
  // contentView should be at 0,0 800x600
  // contentArea should be at 0,0 800x501
  // infobar should be at 0,501 800x60
  // toolbar should be at 0,561 800x39
  [controller_ resizeView:infobar newHeight:60];
  EXPECT_TRUE(NSEqualRects([contentView frame], NSMakeRect(0, 0, 800, 600)));
  EXPECT_TRUE(NSEqualRects([contentArea frame], NSMakeRect(0, 0, 800, 501)));
  EXPECT_TRUE(NSEqualRects([infobar frame], NSMakeRect(0, 501, 800, 60)));
  EXPECT_TRUE(NSEqualRects([toolbar frame], NSMakeRect(0, 561, 800, 39)));

  // Expand the toolbar to 64px and recheck
  // contentView should be at 0,0 800x600
  // contentArea should be at 0,0 800x476
  // infobar should be at 0,476 800x60
  // toolbar should be at 0,536 800x64
  [controller_ resizeView:toolbar newHeight:64];
  EXPECT_TRUE(NSEqualRects([contentView frame], NSMakeRect(0, 0, 800, 600)));
  EXPECT_TRUE(NSEqualRects([contentArea frame], NSMakeRect(0, 0, 800, 476)));
  EXPECT_TRUE(NSEqualRects([infobar frame], NSMakeRect(0, 476, 800, 60)));
  EXPECT_TRUE(NSEqualRects([toolbar frame], NSMakeRect(0, 536, 800, 64)));

  // Add a 30px download shelf and recheck
  // contentView should be at 0,0 800x600
  // download should be at 0,0 800x30
  // contentArea should be at 0,30 800x446
  // infobar should be at 0,476 800x60
  // toolbar should be at 0,536 800x64
  NSView* download = [[controller_ downloadShelf] view];
  [controller_ resizeView:download newHeight:30];
  EXPECT_TRUE(NSEqualRects([contentView frame], NSMakeRect(0, 0, 800, 600)));
  EXPECT_TRUE(NSEqualRects([download frame], NSMakeRect(0, 0, 800, 30)));
  EXPECT_TRUE(NSEqualRects([contentArea frame], NSMakeRect(0, 30, 800, 446)));
  EXPECT_TRUE(NSEqualRects([infobar frame], NSMakeRect(0, 476, 800, 60)));
  EXPECT_TRUE(NSEqualRects([toolbar frame], NSMakeRect(0, 536, 800, 64)));

  // Shrink the infobar to 0px and toolbar to 39px and recheck
  // contentView should be at 0,0 800x600
  // download should be at 0,0 800x30
  // contentArea should be at 0,30 800x531
  // infobar should be at 0,561 800x0
  // toolbar should be at 0,561 800x39
  [controller_ resizeView:infobar newHeight:0];
  [controller_ resizeView:toolbar newHeight:39];
  EXPECT_TRUE(NSEqualRects([contentView frame], NSMakeRect(0, 0, 800, 600)));
  EXPECT_TRUE(NSEqualRects([download frame], NSMakeRect(0, 0, 800, 30)));
  EXPECT_TRUE(NSEqualRects([contentArea frame], NSMakeRect(0, 30, 800, 531)));
  EXPECT_TRUE(NSEqualRects([infobar frame], NSMakeRect(0, 561, 800, 0)));
  EXPECT_TRUE(NSEqualRects([toolbar frame], NSMakeRect(0, 561, 800, 39)));
}

/* TODO(???): test other methods of BrowserWindowController */

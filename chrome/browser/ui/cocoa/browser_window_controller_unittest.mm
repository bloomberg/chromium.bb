// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

#include <memory>

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"

using ::testing::Return;

@interface BrowserWindowController (JustForTesting)
// Already defined in BWC.
- (void)saveWindowPositionIfNeeded;
- (void)layoutSubviews;
@end

@interface BrowserWindowController (ExposedForTesting)
// Implementations are below.
- (void)dontFocusLocationBar:(BOOL)selectAll;
@end

@implementation BrowserWindowController (ExposedForTesting)

- (void)dontFocusLocationBar:(BOOL)selectAll {
}
@end

class BrowserWindowControllerTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ = [[BrowserWindowController alloc] initWithBrowser:browser()
                                                     takeOwnership:NO];
  }

  void TearDown() override {
    [[controller_ nsWindowController] close];
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

TEST_F(BrowserWindowControllerTest, TestTheme) {
  [controller_ userChangedTheme];
}

TEST_F(BrowserWindowControllerTest, TestAdjustWindowHeight) {
  NSWindow* window = [controller_ window];
  NSRect workarea = [[window screen] visibleFrame];

  // Place the window well above the bottom of the screen and try to adjust its
  // height. It should change appropriately (and only downwards). Then get it to
  // shrink by the same amount; it should return to its original state.
  NSRect initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y + 100,
                                   200, 280);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  NSRect finalFrame = [window frame];
  EXPECT_NSNE(finalFrame, initialFrame);
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
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y, 200, 280);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_NSNE(finalFrame, initialFrame);
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame) + 40);
  [controller_ adjustWindowHeightBy:-40];
  finalFrame = [window frame];
  EXPECT_FLOAT_EQ(NSMinY(finalFrame), NSMinY(initialFrame));
  EXPECT_FLOAT_EQ(NSHeight(finalFrame), NSHeight(initialFrame));

  // Put the window slightly offscreen and try again.  The height should not
  // change this time.
  initialFrame = NSMakeRect(workarea.origin.x - 10, 0, 200, 280);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  EXPECT_NSEQ([window frame], initialFrame);
  [controller_ adjustWindowHeightBy:-40];
  EXPECT_NSEQ([window frame], initialFrame);

  // Make the window the same size as the workarea.  Resizing both larger and
  // smaller should have no effect.
  [window setFrame:workarea display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  EXPECT_NSEQ([window frame], workarea);
  [controller_ adjustWindowHeightBy:-40];
  EXPECT_NSEQ([window frame], workarea);

  // Make the window smaller than the workarea and place it near the bottom of
  // the workarea.  The window should grow down until it hits the bottom and
  // then continue to grow up. Then shrink it, and it should return to where it
  // was.
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y + 5,
                            200, 280);
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
  initialFrame = NSMakeRect(workarea.origin.x, workarea.origin.y, 200, 280);
  [window setFrame:initialFrame display:YES];
  [controller_ resetWindowGrowthState];
  [controller_ adjustWindowHeightBy:40];
  finalFrame = [window frame];
  EXPECT_NSNE(finalFrame, initialFrame);
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

// Test that the window uses Auto Layout. Since frame-based layout and Auto
// Layout behave differently in subtle ways, we shouldn't start/stop using it
// accidentally. If we don't want Auto Layout, this test should be changed to
// expect that chromeContentView has no constraints.
TEST_F(BrowserWindowControllerTest, UsesAutoLayout) {
  // If Auto Layout is on, there will be synthesized constraints based on the
  // view's frame and autoresizing mask.
  EXPECT_EQ(0u, [[[controller_ chromeContentView] constraints] count]);
}

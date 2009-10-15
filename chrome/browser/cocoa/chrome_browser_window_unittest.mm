// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/browser_frame_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class ChromeBrowserWindowTest : public PlatformTest {
 public:
  ChromeBrowserWindowTest() {
    // Create a window.
    const NSUInteger mask = NSTitledWindowMask | NSClosableWindowMask |
        NSMiniaturizableWindowMask | NSResizableWindowMask;
    window_.reset([[ChromeBrowserWindow alloc]
                    initWithContentRect:NSMakeRect(0, 0, 800, 600)
                              styleMask:mask
                                backing:NSBackingStoreBuffered
                                  defer:NO]);
    if (DebugUtil::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }
  }

  // Returns a canonical snapshot of the window.
  NSData* WindowContentsAsTIFF() {
    NSRect frame([window_ frame]);
    frame.origin = [window_ convertScreenToBase:frame.origin];

    NSData* pdfData = [window_ dataWithPDFInsideRect:frame];

    // |pdfData| can differ for windows which look the same, so make it
    // canonical.
    NSImage* image = [[[NSImage alloc] initWithData:pdfData] autorelease];
    return [image TIFFRepresentation];
  }

  CocoaNoWindowTestHelper cocoa_helper_;
  scoped_nsobject<ChromeBrowserWindow> window_;
};

// Baseline test that the window creates, displays, closes, and
// releases.
TEST_F(ChromeBrowserWindowTest, ShowAndClose) {
  [window_ display];
}

// Test that undocumented title-hiding API we're using does the job.
TEST_F(ChromeBrowserWindowTest, DoesHideTitle) {
  // The -display calls are not strictly necessary, but they do
  // make it easier to see what's happening when debugging (without
  // them the changes are never flushed to the screen).

  [window_ setTitle:@""];
  [window_ display];
  NSData* emptyTitleData = WindowContentsAsTIFF();

  [window_ setTitle:@"This is a title"];
  [window_ display];
  NSData* thisTitleData = WindowContentsAsTIFF();

  // The default window with a title should look different from the
  // window with an emtpy title.
  EXPECT_FALSE([emptyTitleData isEqualToData:thisTitleData]);

  [window_ setShouldHideTitle:YES];
  [window_ setTitle:@""];
  [window_ display];
  [window_ setTitle:@"This is a title"];
  [window_ display];
  NSData* hiddenTitleData = WindowContentsAsTIFF();

  // With our magic setting, the window with a title should look the
  // same as the window with an empty title.
  EXPECT_TRUE([window_ _isTitleHidden]);
  EXPECT_TRUE([emptyTitleData isEqualToData:hiddenTitleData]);
}

// Test to make sure that our window widgets are in the right place.
TEST_F(ChromeBrowserWindowTest, DISABLED_WindowWidgetLocation) {
  NSCell* closeBoxCell = [window_ accessibilityAttributeValue:
                          NSAccessibilityCloseButtonAttribute];
  NSView* closeBoxControl = [closeBoxCell controlView];
  EXPECT_TRUE(closeBoxControl);
  NSRect closeBoxFrame = [closeBoxControl frame];
  NSRect windowBounds = [window_ frame];
  windowBounds.origin = NSZeroPoint;
  EXPECT_EQ(NSMaxY(closeBoxFrame),
            NSMaxY(windowBounds) -
            kChromeWindowButtonsWithTabStripOffsetFromTop);
  EXPECT_EQ(NSMinX(closeBoxFrame), kChromeWindowButtonsOffsetFromLeft);

  NSCell* miniaturizeCell = [window_ accessibilityAttributeValue:
                             NSAccessibilityMinimizeButtonAttribute];
  NSView* miniaturizeControl = [miniaturizeCell controlView];
  EXPECT_TRUE(miniaturizeControl);
  NSRect miniaturizeFrame = [miniaturizeControl frame];
  EXPECT_EQ(NSMaxY(miniaturizeFrame),
            NSMaxY(windowBounds) -
            kChromeWindowButtonsWithTabStripOffsetFromTop);
  EXPECT_EQ(NSMinX(miniaturizeFrame),
            NSMaxX(closeBoxFrame) + kChromeWindowButtonsInterButtonSpacing);
}

// Test that we actually have a tracking area in place.
TEST_F(ChromeBrowserWindowTest, DISABLED_WindowWidgetTrackingArea) {
  NSCell* closeBoxCell = [window_ accessibilityAttributeValue:
                          NSAccessibilityCloseButtonAttribute];
  NSView* closeBoxControl = [closeBoxCell controlView];
  NSView* frameView = [[window_ contentView] superview];
  NSArray* trackingAreas = [frameView trackingAreas];
  NSPoint point = [closeBoxControl frame].origin;
  point.x += 1;
  point.y += 1;
  BOOL foundArea = NO;
  for (NSTrackingArea* area in trackingAreas) {
    NSRect rect = [area rect];
    foundArea = NSPointInRect(point, rect);
    if (foundArea) {
      EXPECT_TRUE([[area owner] isEqual:window_]);
      break;
    }
  }
  EXPECT_TRUE(foundArea);
}


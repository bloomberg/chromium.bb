// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"

namespace {

// The height of the AppKit menu bar.
const CGFloat kMenuBarHeight = 22;

// Returns the frame of the view in window coordinates.
NSRect FrameInWindow(NSView* view) {
  return [view convertRect:[view bounds] toView:nil];
}

// Returns the min Y of the view in window coordinates.
CGFloat MinYInWindow(NSView* view) {
  return NSMinY(FrameInWindow(view));
}

// Returns the max Y of the view in window coordinates.
CGFloat MaxYInWindow(NSView* view) {
  return NSMaxY(FrameInWindow(view));
}

// Returns the view positioned highest in the toolbar area.
NSView* HighestViewInToolbarArea(BrowserWindowController* controller) {
  return [controller tabStripView];
}

// Returns the view positioned lowest in the toolbar area.
NSView* LowestViewInToolbarArea(BrowserWindowController* controller) {
  return [[controller bookmarkBarController] view];
}

// Check the layout of the views in the toolbar area when none of them overlap.
void CheckToolbarLayoutNoOverlap(BrowserWindowController* controller) {
  EXPECT_EQ(MinYInWindow([controller tabStripView]),
            MaxYInWindow([[controller toolbarController] view]));
  EXPECT_EQ(MinYInWindow([[controller toolbarController] view]),
            MaxYInWindow([[controller bookmarkBarController] view]));
}

// Check the layout of all of the views when none of them overlap.
void CheckLayoutNoOverlap(BrowserWindowController* controller) {
  CheckToolbarLayoutNoOverlap(controller);
  EXPECT_EQ(MinYInWindow([[controller bookmarkBarController] view]),
            MaxYInWindow([[controller infoBarContainerController] view]));
  EXPECT_EQ(MinYInWindow([[controller infoBarContainerController] view]),
            MaxYInWindow([controller tabContentArea]));
}

}  // namespace

// -------------------MockPresentationModeController----------------------------
// Mock of PresentationModeController that eliminates dependencies on AppKit.
@interface MockPresentationModeController : PresentationModeController
@end

@implementation MockPresentationModeController

- (void)setSystemFullscreenModeTo:(base::mac::FullScreenMode)mode {
}

- (CGFloat)floatingBarVerticalOffset {
  return kMenuBarHeight;
}

@end

// -------------------MockBrowserWindowController-------------------------------
// Mock of BrowserWindowController that eliminates dependencies on AppKit.
@interface MockBrowserWindowController : BrowserWindowController {
 @public
  MockPresentationModeController* presentationController_;
  BOOL isInAppKitFullscreen_;
}
@end

@implementation MockBrowserWindowController

// Use the mock presentation controller.
// Superclass override.
- (PresentationModeController*)newPresentationModeControllerWithStyle:
                                   (fullscreen_mac::SlidingStyle)style {
  presentationController_ =
      [[MockPresentationModeController alloc] initWithBrowserController:self
                                                                  style:style];
  return presentationController_;
}

// Manually control whether the floating bar (omnibox, tabstrip, etc.) has
// focus.
// Superclass override.
- (BOOL)floatingBarHasFocus {
  return NO;
}

// Superclass override.
- (BOOL)isInAppKitFullscreen {
  return isInAppKitFullscreen_;
}

// Superclass override.
- (BOOL)placeBookmarkBarBelowInfoBar {
  return NO;
}
@end

// -------------------PresentationModeControllerTest----------------------------
class PresentationModeControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ = [[MockBrowserWindowController alloc] initWithBrowser:browser()
                                                         takeOwnership:NO];
    [[controller_ bookmarkBarController]
        updateState:BookmarkBar::SHOW
         changeType:BookmarkBar::DONT_ANIMATE_STATE_CHANGE];
  }

  virtual void TearDown() OVERRIDE {
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

 public:
  MockBrowserWindowController* controller_;
};

// Tests the layout of views in Canonical Fullscreen (emulating AppKit
// Fullscreen API).
TEST_F(PresentationModeControllerTest, CanonicalFullscreenAppKitLayout) {
  // Check initial layout.
  CGFloat windowHeight = NSHeight([[controller_ window] frame]);
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabStripView]));
  CheckLayoutNoOverlap(controller_);

  // No change after adjusting UI for Canonical Fullscreen.
  controller_->isInAppKitFullscreen_ = YES;
  [controller_
      adjustUIForSlidingFullscreenStyle:fullscreen_mac::OMNIBOX_TABS_PRESENT];
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabStripView]));
  CheckLayoutNoOverlap(controller_);

  // The menu bar is starting to animate in. All views should slide down by a
  // small amount. The content area doesn't change size.
  [controller_->presentationController_ setMenuBarRevealProgress:0.3];
  EXPECT_LT(MaxYInWindow([controller_ tabStripView]), windowHeight - 1);
  EXPECT_GT(MaxYInWindow([controller_ tabStripView]),
            windowHeight - kMenuBarHeight + 1);
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_EQ(MinYInWindow([[controller_ bookmarkBarController] view]),
            MaxYInWindow([[controller_ infoBarContainerController] view]));
  EXPECT_LT(MinYInWindow([[controller_ infoBarContainerController] view]),
            MaxYInWindow([controller_ tabContentArea]));

  // The menu bar is fully visible. All views should slide down by the size of
  // the menu bar. The content area doesn't change size.
  [controller_->presentationController_ setMenuBarRevealProgress:1];
  EXPECT_FLOAT_EQ(windowHeight - kMenuBarHeight,
                  MaxYInWindow([controller_ tabStripView]));
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_EQ(MinYInWindow([[controller_ bookmarkBarController] view]),
            MaxYInWindow([[controller_ infoBarContainerController] view]));
  EXPECT_LT(MinYInWindow([[controller_ infoBarContainerController] view]),
            MaxYInWindow([controller_ tabContentArea]));

  // The menu bar has disappeared. All views should return to normal.
  [controller_->presentationController_ setMenuBarRevealProgress:0];
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabStripView]));
  CheckLayoutNoOverlap(controller_);
}

// Tests the layout of views in Presentation Mode (emulating AppKit Fullscreen
// API).
TEST_F(PresentationModeControllerTest, PresentationModeAppKitLayout) {
  // Check initial layout.
  CGFloat windowHeight = NSHeight([[controller_ window] frame]);
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabStripView]));
  CheckLayoutNoOverlap(controller_);

  // Adjust UI for Presentation Mode.
  controller_->isInAppKitFullscreen_ = YES;
  [controller_
      adjustUIForSlidingFullscreenStyle:fullscreen_mac::OMNIBOX_TABS_HIDDEN];

  // In AppKit Fullscreen, the titlebar disappears. This test can't remove the
  // titlebar without non-trivially changing the view hierarchy. Instead, it
  // adjusts the expectations to sometimes use contentHeight instead of
  // windowHeight (the two should be the same in AppKit Fullscreen).
  CGFloat contentHeight = NSHeight([[[controller_ window] contentView] bounds]);
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_EQ(windowHeight, MinYInWindow(LowestViewInToolbarArea(controller_)));
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabContentArea]));

  // The menu bar is starting to animate in. All views except the content view
  // should slide down by a small amount.
  [controller_->presentationController_ setMenuBarRevealProgress:0.3];
  [controller_->presentationController_ changeToolbarFraction:0.3];
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_LT(MinYInWindow(LowestViewInToolbarArea(controller_)), contentHeight);
  EXPECT_GT(MaxYInWindow(HighestViewInToolbarArea(controller_)), contentHeight);
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabContentArea]));

  // The menu bar is fully visible. All views should slide down by the size of
  // the menu bar.
  [controller_->presentationController_ setMenuBarRevealProgress:1];
  [controller_->presentationController_ changeToolbarFraction:1];
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_EQ(contentHeight, MaxYInWindow(HighestViewInToolbarArea(controller_)));
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabContentArea]));

  // The menu bar has disappeared. All views should return to normal.
  [controller_->presentationController_ setMenuBarRevealProgress:0];
  [controller_->presentationController_ changeToolbarFraction:0];
  CheckToolbarLayoutNoOverlap(controller_);
  EXPECT_EQ(windowHeight, MinYInWindow(LowestViewInToolbarArea(controller_)));
  EXPECT_EQ(windowHeight, MaxYInWindow([controller_ tabContentArea]));
}

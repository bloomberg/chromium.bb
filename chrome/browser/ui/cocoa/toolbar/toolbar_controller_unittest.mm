// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// An NSView that fakes out hitTest:.
@interface HitView : NSView {
  id hitTestReturn_;
}
@end

@implementation HitView

- (void)setHitTestReturn:(id)rtn {
  hitTestReturn_ = rtn;
}

- (NSView *)hitTest:(NSPoint)aPoint {
  return hitTestReturn_;
}

@end


namespace {

class ToolbarControllerTest : public CocoaTest {
 public:

  // Indexes that match the ordering returned by the private ToolbarController
  // |-toolbarViews| method.
  enum {
    kBackIndex, kForwardIndex, kReloadIndex, kHomeIndex,
    kWrenchIndex, kLocationIndex, kBrowserActionContainerViewIndex
  };

  ToolbarControllerTest() {
    Browser* browser = helper_.browser();
    CommandUpdater* updater = browser->command_updater();
    // The default state for the commands is true, set a couple to false to
    // ensure they get picked up correct on initialization
    updater->UpdateCommandEnabled(IDC_BACK, false);
    updater->UpdateCommandEnabled(IDC_FORWARD, false);
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    bar_.reset(
        [[ToolbarController alloc] initWithModel:browser->toolbar_model()
                                        commands:browser->command_updater()
                                         profile:helper_.profile()
                                         browser:browser
                                  resizeDelegate:resizeDelegate_.get()]);
    EXPECT_TRUE([bar_ view]);
    NSView* parent = [test_window() contentView];
    [parent addSubview:[bar_ view]];
  }

  // Make sure the enabled state of the view is the same as the corresponding
  // command in the updater. The views are in the declaration order of outlets.
  void CompareState(CommandUpdater* updater, NSArray* views) {
    EXPECT_EQ(updater->IsCommandEnabled(IDC_BACK),
              [[views objectAtIndex:kBackIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_FORWARD),
              [[views objectAtIndex:kForwardIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_RELOAD),
              [[views objectAtIndex:kReloadIndex] isEnabled] ? true : false);
    EXPECT_EQ(updater->IsCommandEnabled(IDC_HOME),
              [[views objectAtIndex:kHomeIndex] isEnabled] ? true : false);
  }

  BrowserTestHelper helper_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  scoped_nsobject<ToolbarController> bar_;
};

TEST_VIEW(ToolbarControllerTest, [bar_ view])

// Test the initial state that everything is sync'd up
TEST_F(ToolbarControllerTest, InitialState) {
  CommandUpdater* updater = helper_.browser()->command_updater();
  CompareState(updater, [bar_ toolbarViews]);
}

// Make sure a "titlebar only" toolbar with location bar works.
TEST_F(ToolbarControllerTest, TitlebarOnly) {
  NSView* view = [bar_ view];

  [bar_ setHasToolbar:NO hasLocationBar:YES];
  EXPECT_NE(view, [bar_ view]);

  // Simulate a popup going fullscreen and back by performing the reparenting
  // that happens during fullscreen transitions
  NSView* superview = [view superview];
  [view removeFromSuperview];
  [superview addSubview:view];

  [bar_ setHasToolbar:YES hasLocationBar:YES];
  EXPECT_EQ(view, [bar_ view]);

  // Leave it off to make sure that's fine
  [bar_ setHasToolbar:NO hasLocationBar:YES];
}

// Make sure it works in the completely undecorated case.
TEST_F(ToolbarControllerTest, NoLocationBar) {
  NSView* view = [bar_ view];

  [bar_ setHasToolbar:NO hasLocationBar:NO];
  EXPECT_NE(view, [bar_ view]);
  EXPECT_TRUE([[bar_ view] isHidden]);

  // Simulate a popup going fullscreen and back by performing the reparenting
  // that happens during fullscreen transitions
  NSView* superview = [view superview];
  [view removeFromSuperview];
  [superview addSubview:view];
}

// Make some changes to the enabled state of a few of the buttons and ensure
// that we're still in sync.
TEST_F(ToolbarControllerTest, UpdateEnabledState) {
  CommandUpdater* updater = helper_.browser()->command_updater();
  EXPECT_FALSE(updater->IsCommandEnabled(IDC_BACK));
  EXPECT_FALSE(updater->IsCommandEnabled(IDC_FORWARD));
  updater->UpdateCommandEnabled(IDC_BACK, true);
  updater->UpdateCommandEnabled(IDC_FORWARD, true);
  CompareState(updater, [bar_ toolbarViews]);
}

// Focus the location bar and make sure that it's the first responder.
TEST_F(ToolbarControllerTest, FocusLocation) {
  NSWindow* window = test_window();
  [window makeFirstResponder:[window contentView]];
  EXPECT_EQ([window firstResponder], [window contentView]);
  [bar_ focusLocationBar:YES];
  EXPECT_NE([window firstResponder], [window contentView]);
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  EXPECT_EQ([window firstResponder], [(id)locationBar currentEditor]);
}

TEST_F(ToolbarControllerTest, LoadingState) {
  // In its initial state, the reload button has a tag of
  // IDC_RELOAD. When loading, it should be IDC_STOP.
  NSButton* reload = [[bar_ toolbarViews] objectAtIndex:kReloadIndex];
  EXPECT_EQ([reload tag], IDC_RELOAD);
  [bar_ setIsLoading:YES force:YES];
  EXPECT_EQ([reload tag], IDC_STOP);
  [bar_ setIsLoading:NO force:YES];
  EXPECT_EQ([reload tag], IDC_RELOAD);
}

// Check that toggling the state of the home button changes the visible
// state of the home button and moves the other items accordingly.
TEST_F(ToolbarControllerTest, ToggleHome) {
  PrefService* prefs = helper_.profile()->GetPrefs();
  bool showHome = prefs->GetBoolean(prefs::kShowHomeButton);
  NSView* homeButton = [[bar_ toolbarViews] objectAtIndex:kHomeIndex];
  EXPECT_EQ(showHome, ![homeButton isHidden]);

  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  NSRect originalLocationBarFrame = [locationBar frame];

  // Toggle the pref and make sure the button changed state and the other
  // views moved.
  prefs->SetBoolean(prefs::kShowHomeButton, !showHome);
  EXPECT_EQ(showHome, [homeButton isHidden]);
  EXPECT_NE(NSMinX(originalLocationBarFrame), NSMinX([locationBar frame]));
  EXPECT_NE(NSWidth(originalLocationBarFrame), NSWidth([locationBar frame]));
}

// Ensure that we don't toggle the buttons when we have a strip marked as not
// having the full toolbar. Also ensure that the location bar doesn't change
// size.
TEST_F(ToolbarControllerTest, DontToggleWhenNoToolbar) {
  [bar_ setHasToolbar:NO hasLocationBar:YES];
  NSView* homeButton = [[bar_ toolbarViews] objectAtIndex:kHomeIndex];
  NSView* locationBar = [[bar_ toolbarViews] objectAtIndex:kLocationIndex];
  NSRect locationBarFrame = [locationBar frame];
  EXPECT_EQ([homeButton isHidden], YES);
  [bar_ showOptionalHomeButton];
  EXPECT_EQ([homeButton isHidden], YES);
  NSRect newLocationBarFrame = [locationBar frame];
  EXPECT_TRUE(NSEqualRects(locationBarFrame, newLocationBarFrame));
  newLocationBarFrame = [locationBar frame];
  EXPECT_TRUE(NSEqualRects(locationBarFrame, newLocationBarFrame));
}

TEST_F(ToolbarControllerTest, BookmarkBubblePoint) {
  const NSPoint starPoint = [bar_ bookmarkBubblePoint];
  const NSRect barFrame =
      [[bar_ view] convertRect:[[bar_ view] bounds] toView:nil];

  // Make sure the star is completely inside the location bar.
  EXPECT_TRUE(NSPointInRect(starPoint, barFrame));
}

TEST_F(ToolbarControllerTest, HoverButtonForEvent) {
  scoped_nsobject<HitView> view([[HitView alloc]
                                  initWithFrame:NSMakeRect(0,0,100,100)]);
  [bar_ setView:view];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:NSMakePoint(10,10)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0.0];

  // NOT a match.
  [view setHitTestReturn:bar_.get()];
  EXPECT_FALSE([bar_ hoverButtonForEvent:event]);

  // Not yet...
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [view setHitTestReturn:button];
  EXPECT_FALSE([bar_ hoverButtonForEvent:event]);

  // Now!
  scoped_nsobject<GradientButtonCell> cell([[GradientButtonCell alloc] init]);
  [button setCell:cell.get()];
  EXPECT_TRUE([bar_ hoverButtonForEvent:nil]);
}

}  // namespace

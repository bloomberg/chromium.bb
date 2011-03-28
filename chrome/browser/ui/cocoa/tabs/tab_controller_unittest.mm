// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Implements the target interface for the tab, which gets sent messages when
// the tab is clicked on by the user and when its close box is clicked.
@interface TabControllerTestTarget : NSObject<TabControllerTarget> {
 @private
  bool selected_;
  bool closed_;
}
- (bool)selected;
- (bool)closed;
@end

@implementation TabControllerTestTarget
- (bool)selected {
  return selected_;
}
- (bool)closed {
  return closed_;
}
- (void)selectTab:(id)sender {
  selected_ = true;
}
- (void)closeTab:(id)sender {
  closed_ = true;
}
- (void)mouseTimer:(NSTimer*)timer {
  // Fire the mouseUp to break the TabView drag loop.
  NSEvent* current = [NSApp currentEvent];
  NSWindow* window = [timer userInfo];
  NSEvent* up = [NSEvent mouseEventWithType:NSLeftMouseUp
                                   location:[current locationInWindow]
                              modifierFlags:0
                                  timestamp:[current timestamp]
                               windowNumber:[window windowNumber]
                                    context:nil
                                eventNumber:0
                                 clickCount:1
                                   pressure:1.0];
  [window postEvent:up atStart:YES];
}
- (void)commandDispatch:(TabStripModel::ContextMenuCommand)command
          forController:(TabController*)controller {
}
- (BOOL)isCommandEnabled:(TabStripModel::ContextMenuCommand)command
           forController:(TabController*)controller {
  return NO;
}
@end

namespace {

CGFloat LeftMargin(NSRect superFrame, NSRect subFrame) {
  return NSMinX(subFrame) - NSMinX(superFrame);
}

CGFloat RightMargin(NSRect superFrame, NSRect subFrame) {
  return NSMaxX(superFrame) - NSMaxX(subFrame);
}

// The dragging code in TabView makes heavy use of autorelease pools so
// inherit from CocoaTest to have one created for us.
class TabControllerTest : public CocoaTest {
 public:
  TabControllerTest() { }
};

// Tests creating the controller, sticking it in a window, and removing it.
TEST_F(TabControllerTest, Creation) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  EXPECT_TRUE([controller tabView]);
  EXPECT_EQ([[controller view] window], window);
  [[controller view] display];  // Test drawing to ensure nothing leaks/crashes.
  [[controller view] removeFromSuperview];
}

// Tests sending it a close message and ensuring that the target/action get
// called. Mimics the user clicking on the close button in the tab.
TEST_F(TabControllerTest, Close) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  scoped_nsobject<TabControllerTestTarget> target(
      [[TabControllerTestTarget alloc] init]);
  EXPECT_FALSE([target closed]);
  [controller setTarget:target];
  EXPECT_EQ(target.get(), [controller target]);

  [controller closeTab:nil];
  EXPECT_TRUE([target closed]);

  [[controller view] removeFromSuperview];
}

// Tests setting the |selected| property via code.
TEST_F(TabControllerTest, APISelection) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_FALSE([controller selected]);
  [controller setSelected:YES];
  EXPECT_TRUE([controller selected]);

  [[controller view] removeFromSuperview];
}

// Tests that setting the title of a tab sets the tooltip as well.
TEST_F(TabControllerTest, ToolTip) {
  NSWindow* window = test_window();

  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_TRUE([[controller toolTip] length] == 0);
  NSString *tooltip_string = @"Some text to use as a tab title";
  [controller setTitle:tooltip_string];
  EXPECT_NSEQ(tooltip_string, [controller toolTip]);
}

// Tests setting the |loading| property via code.
TEST_F(TabControllerTest, Loading) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_EQ(kTabDone, [controller loadingState]);
  [controller setLoadingState:kTabWaiting];
  EXPECT_EQ(kTabWaiting, [controller loadingState]);
  [controller setLoadingState:kTabLoading];
  EXPECT_EQ(kTabLoading, [controller loadingState]);
  [controller setLoadingState:kTabDone];
  EXPECT_EQ(kTabDone, [controller loadingState]);

  [[controller view] removeFromSuperview];
}

// Tests selecting the tab with the mouse click and ensuring the target/action
// get called.
TEST_F(TabControllerTest, UserSelection) {
  NSWindow* window = test_window();

  // Create a tab at a known location in the window that we can click on
  // to activate selection.
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  NSRect frame = [[controller view] frame];
  frame.size.width = [TabController minTabWidth];
  frame.origin = NSMakePoint(0, 0);
  [[controller view] setFrame:frame];

  // Set the target and action.
  scoped_nsobject<TabControllerTestTarget> target(
      [[TabControllerTestTarget alloc] init]);
  EXPECT_FALSE([target selected]);
  [controller setTarget:target];
  [controller setAction:@selector(selectTab:)];
  EXPECT_EQ(target.get(), [controller target]);
  EXPECT_EQ(@selector(selectTab:), [controller action]);

  // In order to track a click, we have to fake a mouse down and a mouse
  // up, but the down goes into a tight drag loop. To break the loop, we have
  // to fire a timer that sends a mouse up event while the "drag" is ongoing.
  [NSTimer scheduledTimerWithTimeInterval:0.1
                                   target:target.get()
                                 selector:@selector(mouseTimer:)
                                 userInfo:window
                                  repeats:NO];
  NSEvent* current = [NSApp currentEvent];
  NSPoint click_point = NSMakePoint(frame.size.width / 2,
                                    frame.size.height / 2);
  NSEvent* down = [NSEvent mouseEventWithType:NSLeftMouseDown
                                     location:click_point
                                modifierFlags:0
                                    timestamp:[current timestamp]
                                 windowNumber:[window windowNumber]
                                      context:nil
                                  eventNumber:0
                                   clickCount:1
                                     pressure:1.0];
  [[controller view] mouseDown:down];

  // Check our target was told the tab got selected.
  EXPECT_TRUE([target selected]);

  [[controller view] removeFromSuperview];
}

TEST_F(TabControllerTest, IconCapacity) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  int cap = [controller iconCapacity];
  EXPECT_GE(cap, 1);

  NSRect frame = [[controller view] frame];
  frame.size.width += 500;
  [[controller view] setFrame:frame];
  int newcap = [controller iconCapacity];
  EXPECT_GT(newcap, cap);
}

TEST_F(TabControllerTest, ShouldShowIcon) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  int cap = [controller iconCapacity];
  EXPECT_GT(cap, 0);

  // Tab is minimum width, both icon and close box should be hidden.
  NSRect frame = [[controller view] frame];
  frame.size.width = [TabController minTabWidth];
  [[controller view] setFrame:frame];
  EXPECT_FALSE([controller shouldShowIcon]);
  EXPECT_FALSE([controller shouldShowCloseButton]);

  // Setting the icon when tab is at min width should not show icon (bug 18359).
  scoped_nsobject<NSView> newIcon(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)]);
  [controller setIconView:newIcon.get()];
  EXPECT_TRUE([newIcon isHidden]);

  // Tab is at selected minimum width. Since it's selected, the close box
  // should be visible.
  [controller setSelected:YES];
  frame = [[controller view] frame];
  frame.size.width = [TabController minSelectedTabWidth];
  [[controller view] setFrame:frame];
  EXPECT_FALSE([controller shouldShowIcon]);
  EXPECT_TRUE([newIcon isHidden]);
  EXPECT_TRUE([controller shouldShowCloseButton]);

  // Test expanding the tab to max width and ensure the icon and close box
  // get put back, even when de-selected.
  frame.size.width = [TabController maxTabWidth];
  [[controller view] setFrame:frame];
  EXPECT_TRUE([controller shouldShowIcon]);
  EXPECT_FALSE([newIcon isHidden]);
  EXPECT_TRUE([controller shouldShowCloseButton]);
  [controller setSelected:NO];
  EXPECT_TRUE([controller shouldShowIcon]);
  EXPECT_TRUE([controller shouldShowCloseButton]);

  cap = [controller iconCapacity];
  EXPECT_GT(cap, 0);
}

TEST_F(TabControllerTest, Menu) {
  NSWindow* window = test_window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  int cap = [controller iconCapacity];
  EXPECT_GT(cap, 0);

  // Asking the view for its menu should yield a valid menu.
  NSMenu* menu = [[controller view] menu];
  EXPECT_TRUE(menu);
  EXPECT_GT([menu numberOfItems], 0);
}

// Tests that the title field is correctly positioned and sized when the
// view is resized.
TEST_F(TabControllerTest, TitleViewLayout) {
  NSWindow* window = test_window();

  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  NSRect tabFrame = [[controller view] frame];
  tabFrame.size.width = [TabController maxTabWidth];
  [[controller view] setFrame:tabFrame];

  const NSRect originalTabFrame = [[controller view] frame];
  const NSRect originalIconFrame = [[controller iconView] frame];
  const NSRect originalCloseFrame = [[controller closeButton] frame];
  const NSRect originalTitleFrame = [[controller titleView] frame];

  // Sanity check the start state.
  EXPECT_FALSE([[controller iconView] isHidden]);
  EXPECT_FALSE([[controller closeButton] isHidden]);
  EXPECT_GT(NSWidth([[controller view] frame]),
            NSWidth([[controller titleView] frame]));

  // Resize the tab so that that the it shrinks.
  tabFrame.size.width = [TabController minTabWidth];
  [[controller view] setFrame:tabFrame];

  // The icon view and close button should be hidden and the title view should
  // be resize to take up their space.
  EXPECT_TRUE([[controller iconView] isHidden]);
  EXPECT_TRUE([[controller closeButton] isHidden]);
  EXPECT_GT(NSWidth([[controller view] frame]),
            NSWidth([[controller titleView] frame]));
  EXPECT_EQ(LeftMargin(originalTabFrame, originalIconFrame),
            LeftMargin([[controller view] frame],
                       [[controller titleView] frame]));
  EXPECT_EQ(RightMargin(originalTabFrame, originalCloseFrame),
            RightMargin([[controller view] frame],
                        [[controller titleView] frame]));

  // Resize the tab so that that the it grows.
  tabFrame.size.width = [TabController maxTabWidth] * 0.75;
  [[controller view] setFrame:tabFrame];

  // The icon view and close button should be visible again and the title view
  // should be resized to make room for them.
  EXPECT_FALSE([[controller iconView] isHidden]);
  EXPECT_FALSE([[controller closeButton] isHidden]);
  EXPECT_GT(NSWidth([[controller view] frame]),
            NSWidth([[controller titleView] frame]));
  EXPECT_EQ(LeftMargin(originalTabFrame, originalTitleFrame),
            LeftMargin([[controller view] frame],
                       [[controller titleView] frame]));
  EXPECT_EQ(RightMargin(originalTabFrame, originalTitleFrame),
            RightMargin([[controller view] frame],
                        [[controller titleView] frame]));
}

}  // namespace

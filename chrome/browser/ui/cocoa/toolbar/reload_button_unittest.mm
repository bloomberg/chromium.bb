// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/toolbar/reload_button.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/test_event_utils.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol TargetActionMock <NSObject>
- (void)anAction:(id)sender;
@end

namespace {

class ReloadButtonTest : public CocoaTest {
 public:
  ReloadButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 20, 20);
    scoped_nsobject<ReloadButton> button(
        [[ReloadButton alloc] initWithFrame:frame]);
    button_ = button.get();

    // Set things up so unit tests have a reliable baseline.
    [button_ setTag:IDC_RELOAD];
    [button_ awakeFromNib];

    [[test_window() contentView] addSubview:button_];
  }

  ReloadButton* button_;
};

TEST_VIEW(ReloadButtonTest, button_)

// Test that mouse-tracking is setup and does the right thing.
TEST_F(ReloadButtonTest, IsMouseInside) {
  EXPECT_TRUE([[button_ trackingAreas] containsObject:[button_ trackingArea]]);

  EXPECT_FALSE([button_ isMouseInside]);
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ mouseExited:nil];
}

// Verify that multiple clicks do not result in multiple messages to
// the target.
TEST_F(ReloadButtonTest, IgnoredMultiClick) {
  id mock_target = [OCMockObject mockForProtocol:@protocol(TargetActionMock)];
  [button_ setTarget:mock_target];
  [button_ setAction:@selector(anAction:)];

  // Expect the action once.
  [[mock_target expect] anAction:button_];

  const std::pair<NSEvent*,NSEvent*> click_one =
      test_event_utils::MouseClickInView(button_, 1);
  const std::pair<NSEvent*,NSEvent*> click_two =
      test_event_utils::MouseClickInView(button_, 2);
  [NSApp postEvent:click_one.second atStart:YES];
  [button_ mouseDown:click_one.first];
  [NSApp postEvent:click_two.second atStart:YES];
  [button_ mouseDown:click_two.first];

  [button_ setTarget:nil];
}

TEST_F(ReloadButtonTest, UpdateTag) {
  [button_ setTag:IDC_STOP];

  [button_ updateTag:IDC_RELOAD];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
  NSImage* reloadImage = [button_ image];
  NSString* const reloadToolTip = [button_ toolTip];

  [button_ updateTag:IDC_STOP];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  NSImage* stopImage = [button_ image];
  NSString* const stopToolTip = [button_ toolTip];
  EXPECT_NSNE(reloadImage, stopImage);
  EXPECT_NSNE(reloadToolTip, stopToolTip);

  [button_ updateTag:IDC_RELOAD];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
  EXPECT_NSEQ(reloadImage, [button_ image]);
  EXPECT_NSEQ(reloadToolTip, [button_ toolTip]);
}

// Test that when forcing the mode, it takes effect immediately,
// regardless of whether the mouse is hovering.
TEST_F(ReloadButtonTest, SetIsLoadingForce) {
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);

  // Changes to stop immediately.
  [button_ setIsLoading:YES force:YES];
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  // Changes to reload immediately.
  [button_ setIsLoading:NO force:YES];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);

  // Changes to stop immediately when the mouse is hovered, and
  // doesn't change when the mouse exits.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:YES force:YES];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  [button_ mouseExited:nil];
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  // Changes to reload immediately when the mouse is hovered, and
  // doesn't change when the mouse exits.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:NO force:YES];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
  [button_ mouseExited:nil];
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
}

// Test that without force, stop mode is set immediately, but reload
// is affected by the hover status.
TEST_F(ReloadButtonTest, SetIsLoadingNoForceUnHover) {
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);

  // Changes to stop immediately when the mouse is not hovering.
  [button_ setIsLoading:YES force:NO];
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  // Changes to reload immediately when the mouse is not hovering.
  [button_ setIsLoading:NO force:NO];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);

  // Changes to stop immediately when the mouse is hovered, and
  // doesn't change when the mouse exits.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:YES force:NO];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  [button_ mouseExited:nil];
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  // Does not change to reload immediately when the mouse is hovered,
  // changes when the mouse exits.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:NO force:NO];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  [button_ mouseExited:nil];
  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
}

// Test that without force, stop mode is set immediately, and reload
// will be set after a timeout.
// TODO(shess): Reenable, http://crbug.com/61485
TEST_F(ReloadButtonTest, DISABLED_SetIsLoadingNoForceTimeout) {
  // When the event loop first spins, some delayed tracking-area setup
  // is done, which causes -mouseExited: to be called.  Spin it at
  // least once, and dequeue any pending events.
  // TODO(shess): It would be more reasonable to have an MockNSTimer
  // factory for the class to use, which this code could fire
  // directly.
  while ([NSApp nextEventMatchingMask:NSAnyEventMask
                            untilDate:nil
                               inMode:NSDefaultRunLoopMode
                              dequeue:YES]) {
  }

  const NSTimeInterval kShortTimeout = 0.1;
  [ReloadButton setPendingReloadTimeout:kShortTimeout];

  EXPECT_FALSE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);

  // Move the mouse into the button and press it.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:YES force:NO];
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  // Does not change to reload immediately when the mouse is hovered.
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:NO force:NO];
  EXPECT_TRUE([button_ isMouseInside]);
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  EXPECT_TRUE([button_ isMouseInside]);

  // Spin event loop until the timeout passes.
  NSDate* pastTimeout = [NSDate dateWithTimeIntervalSinceNow:2 * kShortTimeout];
  [NSApp nextEventMatchingMask:NSAnyEventMask
                     untilDate:pastTimeout
                        inMode:NSDefaultRunLoopMode
                       dequeue:NO];

  // Mouse is still hovered, button is in reload mode.  If the mouse
  // is no longer hovered, see comment at top of function.
  EXPECT_TRUE([button_ isMouseInside]);
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
}

// Test that pressing stop after reload mode has been requested
// doesn't forward the stop message.
TEST_F(ReloadButtonTest, StopAfterReloadSet) {
  id mock_target = [OCMockObject mockForProtocol:@protocol(TargetActionMock)];
  [button_ setTarget:mock_target];
  [button_ setAction:@selector(anAction:)];

  EXPECT_FALSE([button_ isMouseInside]);

  // Get to stop mode.
  [button_ setIsLoading:YES force:YES];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  EXPECT_TRUE([button_ isEnabled]);

  // Expect the action once.
  [[mock_target expect] anAction:button_];

  // Clicking in stop mode should send the action and transition to
  // reload mode.
  const std::pair<NSEvent*,NSEvent*> click =
      test_event_utils::MouseClickInView(button_, 1);
  [NSApp postEvent:click.second atStart:YES];
  [button_ mouseDown:click.first];
  EXPECT_EQ(IDC_RELOAD, [button_ tag]);
  EXPECT_TRUE([button_ isEnabled]);

  // Get back to stop mode.
  [button_ setIsLoading:YES force:YES];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  EXPECT_TRUE([button_ isEnabled]);

  // If hover prevented reload mode immediately taking effect, clicks should do
  // nothing, because the button should be disabled.
  [button_ mouseEntered:nil];
  EXPECT_TRUE([button_ isMouseInside]);
  [button_ setIsLoading:NO force:NO];
  EXPECT_EQ(IDC_STOP, [button_ tag]);
  EXPECT_FALSE([button_ isEnabled]);
  [NSApp postEvent:click.second atStart:YES];
  [button_ mouseDown:click.first];
  EXPECT_EQ(IDC_STOP, [button_ tag]);

  [button_ setTarget:nil];
}

}  // namespace

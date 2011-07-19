// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/wrench_menu/menu_tracked_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This test does not test what you'd think it does. Testing around event
// tracking run loops is probably not worh the effort when the size of the
// helper MakeEvent() is larger than the class being tested. If we ever figure
// out a good way to test event tracking, this should be revisited.

@interface MenuTrackedButtonTestReceiver : NSObject {
 @public
  BOOL didThat_;
}
- (void)doThat:(id)sender;
@end
@implementation MenuTrackedButtonTestReceiver
- (void)doThat:(id)sender {
  didThat_ = YES;
}
@end


class MenuTrackedButtonTest : public CocoaTest {
 public:
  MenuTrackedButtonTest() : event_number_(0) {}

  void SetUp() {
    listener_.reset([[MenuTrackedButtonTestReceiver alloc] init]);
    button_.reset(
        [[MenuTrackedButton alloc] initWithFrame:NSMakeRect(10, 10, 50, 50)]);
    [[test_window() contentView] addSubview:button()];
    [button_ setTarget:listener()];
    [button_ setAction:@selector(doThat:)];
  }

  // Creates an event of |type|, with |location| in test_window()'s coordinates.
  NSEvent* MakeEvent(NSEventType type, NSPoint location) {
    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    location = [test_window() convertBaseToScreen:location];
    if (type == NSMouseEntered || type == NSMouseExited) {
      return [NSEvent enterExitEventWithType:type
                                    location:location
                               modifierFlags:0
                                   timestamp:now
                                windowNumber:[test_window() windowNumber]
                                     context:nil
                                 eventNumber:event_number_++
                              trackingNumber:0
                                    userData:nil];
    } else {
      return [NSEvent mouseEventWithType:type
                                location:location
                           modifierFlags:0
                               timestamp:now
                            windowNumber:[test_window() windowNumber]
                                 context:nil
                             eventNumber:event_number_++
                              clickCount:1
                                pressure:1.0];
    }
  }

  MenuTrackedButtonTestReceiver* listener() { return listener_.get(); }
  NSButton* button() { return button_.get(); }

  scoped_nsobject<MenuTrackedButtonTestReceiver> listener_;
  scoped_nsobject<MenuTrackedButton> button_;
  NSInteger event_number_;
};

// User mouses over and then off.
TEST_F(MenuTrackedButtonTest, DISABLED_EnterExit) {
  [NSApp postEvent:MakeEvent(NSMouseEntered, NSMakePoint(11, 11)) atStart:YES];
  [NSApp postEvent:MakeEvent(NSMouseExited, NSMakePoint(9, 9)) atStart:YES];
  EXPECT_FALSE(listener()->didThat_);
}

// User mouses over, clicks, drags, and exits.
TEST_F(MenuTrackedButtonTest, DISABLED_EnterDragExit) {
  [NSApp postEvent:MakeEvent(NSMouseEntered, NSMakePoint(11, 11)) atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDown, NSMakePoint(12, 12)) atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDragged, NSMakePoint(13, 11))
           atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDragged, NSMakePoint(13, 10))
           atStart:YES];
  [NSApp postEvent:MakeEvent(NSMouseExited, NSMakePoint(13, 9)) atStart:YES];
  EXPECT_FALSE(listener()->didThat_);
}

// User mouses over, clicks, drags, and releases.
TEST_F(MenuTrackedButtonTest, DISABLED_EnterDragUp) {
  [NSApp postEvent:MakeEvent(NSMouseEntered, NSMakePoint(11, 11)) atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDown, NSMakePoint(12, 12)) atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDragged, NSMakePoint(13, 13))
           atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseUp, NSMakePoint(14, 14)) atStart:YES];
  EXPECT_TRUE(listener()->didThat_);
}

// User drags in and releases.
TEST_F(MenuTrackedButtonTest, DISABLED_DragUp) {
  [NSApp postEvent:MakeEvent(NSLeftMouseDragged, NSMakePoint(11, 11))
           atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseDragged, NSMakePoint(12, 12))
           atStart:YES];
  [NSApp postEvent:MakeEvent(NSLeftMouseUp, NSMakePoint(13, 13))
           atStart:YES];
  EXPECT_TRUE(listener()->didThat_);
}

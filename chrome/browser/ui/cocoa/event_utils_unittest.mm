// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/objc-class.h>

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/cocoa/test_event_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/events.h"

// We provide a donor class with a specially modified |modifierFlags|
// implementation that we swap with NSEvent's. This is because we can't create a
// NSEvent that represents a middle click with modifiers.
@interface TestEvent : NSObject
@end
@implementation TestEvent
- (NSUInteger)modifierFlags { return NSShiftKeyMask; }
@end

namespace {

class EventUtilsTest : public CocoaTest {
};

TEST_F(EventUtilsTest, TestWindowOpenDispositionFromNSEvent) {
  // Left Click = same tab.
  NSEvent* me = test_event_utils::MakeMouseEvent(NSLeftMouseUp, 0);
  EXPECT_EQ(CURRENT_TAB, event_utils::WindowOpenDispositionFromNSEvent(me));

  // Middle Click = new background tab.
  me = test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0);
  EXPECT_EQ(NEW_BACKGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Shift+Middle Click = new foreground tab.
  {
    ScopedClassSwizzler swizzler([NSEvent class], [TestEvent class],
                                 @selector(modifierFlags));
    me = test_event_utils::MakeMouseEvent(NSOtherMouseUp, NSShiftKeyMask);
    EXPECT_EQ(NEW_FOREGROUND_TAB,
              event_utils::WindowOpenDispositionFromNSEvent(me));
  }

  // Cmd+Left Click = new background tab.
  me = test_event_utils::MakeMouseEvent(NSLeftMouseUp, NSCommandKeyMask);
  EXPECT_EQ(NEW_BACKGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Cmd+Shift+Left Click = new foreground tab.
  me = test_event_utils::MakeMouseEvent(NSLeftMouseUp, NSCommandKeyMask | NSShiftKeyMask);
  EXPECT_EQ(NEW_FOREGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Shift+Left Click = new window
  me = test_event_utils::MakeMouseEvent(NSLeftMouseUp, NSShiftKeyMask);
  EXPECT_EQ(NEW_WINDOW, event_utils::WindowOpenDispositionFromNSEvent(me));
}


TEST_F(EventUtilsTest, TestEventFlagsFromNSEvent) {
  // Left click.
  NSEvent* left = test_event_utils::MakeMouseEvent(NSLeftMouseUp, 0);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN,
            event_utils::EventFlagsFromNSEvent(left));

  // Right click.
  NSEvent* right = test_event_utils::MakeMouseEvent(NSRightMouseUp, 0);
  EXPECT_EQ(ui::EF_RIGHT_BUTTON_DOWN,
            event_utils::EventFlagsFromNSEvent(right));

  // Middle click.
  NSEvent* middle = test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0);
  EXPECT_EQ(ui::EF_MIDDLE_BUTTON_DOWN,
            event_utils::EventFlagsFromNSEvent(middle));

  // Caps + Left
  NSEvent* caps = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSAlphaShiftKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_CAPS_LOCK_DOWN,
            event_utils::EventFlagsFromNSEvent(caps));

  // Shift + Left
  NSEvent* shift = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSShiftKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_SHIFT_DOWN,
            event_utils::EventFlagsFromNSEvent(shift));

  // Ctrl + Left
  NSEvent* ctrl = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSControlKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_CONTROL_DOWN,
            event_utils::EventFlagsFromNSEvent(ctrl));

  // Alt + Left
  NSEvent* alt = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSAlternateKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_ALT_DOWN,
            event_utils::EventFlagsFromNSEvent(alt));

  // Cmd + Left
  NSEvent* cmd = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSCommandKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_COMMAND_DOWN,
            event_utils::EventFlagsFromNSEvent(cmd));

  // Shift + Ctrl + Left
  NSEvent* shiftctrl = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSShiftKeyMask | NSControlKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
            event_utils::EventFlagsFromNSEvent(shiftctrl));

  // Cmd + Alt + Right
  NSEvent* cmdalt = test_event_utils::MakeMouseEvent(
    NSLeftMouseUp, NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_EQ(ui::EF_LEFT_BUTTON_DOWN | ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
            event_utils::EventFlagsFromNSEvent(cmdalt));
}

}  // namespace

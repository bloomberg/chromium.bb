// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/objc-class.h>

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/test/cocoa_test_event_utils.h"

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
  NSEvent* me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(CURRENT_TAB, event_utils::WindowOpenDispositionFromNSEvent(me));

  // Middle Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp, 0);
  EXPECT_EQ(NEW_BACKGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Shift+Middle Click = new foreground tab.
  {
    ScopedClassSwizzler swizzler([NSEvent class], [TestEvent class],
                                 @selector(modifierFlags));
    me = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                    NSShiftKeyMask);
    EXPECT_EQ(NEW_FOREGROUND_TAB,
              event_utils::WindowOpenDispositionFromNSEvent(me));
  }

  // Cmd+Left Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                  NSCommandKeyMask);
  EXPECT_EQ(NEW_BACKGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Cmd+Shift+Left Click = new foreground tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                              NSCommandKeyMask |
                                              NSShiftKeyMask);
  EXPECT_EQ(NEW_FOREGROUND_TAB,
            event_utils::WindowOpenDispositionFromNSEvent(me));

  // Shift+Left Click = new window
  me = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                  NSShiftKeyMask);
  EXPECT_EQ(NEW_WINDOW, event_utils::WindowOpenDispositionFromNSEvent(me));
}


TEST_F(EventUtilsTest, TestEventFlagsFromNSEvent) {
  // Left click.
  NSEvent* left = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event_utils::EventFlagsFromNSEvent(left));

  // Right click.
  NSEvent* right = cocoa_test_event_utils::MouseEventWithType(NSRightMouseUp,
                                                              0);
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON,
            event_utils::EventFlagsFromNSEvent(right));

  // Middle click.
  NSEvent* middle = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                               0);
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON,
            event_utils::EventFlagsFromNSEvent(middle));

  // Caps + Left
  NSEvent* caps =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSAlphaShiftKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CAPS_LOCK_DOWN,
            event_utils::EventFlagsFromNSEvent(caps));

  // Shift + Left
  NSEvent* shift = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                              NSShiftKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
            event_utils::EventFlagsFromNSEvent(shift));

  // Ctrl + Left
  NSEvent* ctrl = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                             NSControlKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN,
            event_utils::EventFlagsFromNSEvent(ctrl));

  // Alt + Left
  NSEvent* alt = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSAlternateKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN,
            event_utils::EventFlagsFromNSEvent(alt));

  // Cmd + Left
  NSEvent* cmd = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSCommandKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN,
            event_utils::EventFlagsFromNSEvent(cmd));

  // Shift + Ctrl + Left
  NSEvent* shiftctrl =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSShiftKeyMask |
                                                 NSControlKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
            event_utils::EventFlagsFromNSEvent(shiftctrl));

  // Cmd + Alt + Right
  NSEvent* cmdalt =
      cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                 NSCommandKeyMask |
                                                 NSAlternateKeyMask);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
            event_utils::EventFlagsFromNSEvent(cmdalt));
}

}  // namespace

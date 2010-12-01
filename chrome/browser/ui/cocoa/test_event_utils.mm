// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/test_event_utils.h"

ScopedClassSwizzler::ScopedClassSwizzler(Class target, Class source,
                                         SEL selector) {
  old_selector_impl_ = class_getInstanceMethod(target, selector);
  new_selector_impl_ = class_getInstanceMethod(source, selector);
  method_exchangeImplementations(old_selector_impl_, new_selector_impl_);
}

ScopedClassSwizzler::~ScopedClassSwizzler() {
  method_exchangeImplementations(old_selector_impl_, new_selector_impl_);
}

namespace test_event_utils {

NSEvent* MouseEventAtPoint(NSPoint point, NSEventType type,
                           NSUInteger modifiers) {
  if (type == NSOtherMouseUp) {
    // To synthesize middle clicks we need to create a CGEvent with the
    // "center" button flags so that our resulting NSEvent will have the
    // appropriate buttonNumber field. NSEvent provides no way to create a
    // mouse event with a buttonNumber directly.
    CGPoint location = { point.x, point.y };
    CGEventRef cg_event = CGEventCreateMouseEvent(NULL, kCGEventOtherMouseUp,
                                                  location,
                                                  kCGMouseButtonCenter);
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
    CFRelease(cg_event);
    return event;
  }
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:0
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:1.0];
}

NSEvent* MakeMouseEvent(NSEventType type, NSUInteger modifiers) {
  return MouseEventAtPoint(NSMakePoint(0, 0), type, modifiers);
}

static NSEvent* MouseEventAtPointInWindow(NSPoint point,
                                          NSEventType type,
                                          NSWindow* window,
                                          NSUInteger clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:clickCount
                            pressure:1.0];
}

NSEvent* LeftMouseDownAtPointInWindow(NSPoint point, NSWindow* window) {
  return MouseEventAtPointInWindow(point, NSLeftMouseDown, window, 1);
}

NSEvent* LeftMouseDownAtPoint(NSPoint point) {
  return LeftMouseDownAtPointInWindow(point, nil);
}

std::pair<NSEvent*,NSEvent*> MouseClickInView(NSView* view,
                                              NSUInteger clickCount) {
  const NSRect bounds = [view convertRect:[view bounds] toView:nil];
  const NSPoint mid_point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  NSEvent* down = MouseEventAtPointInWindow(mid_point, NSLeftMouseDown,
                                            [view window], clickCount);
  NSEvent* up = MouseEventAtPointInWindow(mid_point, NSLeftMouseUp,
                                          [view window], clickCount);
  return std::make_pair(down, up);
}

}  // namespace test_event_utils

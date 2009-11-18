// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BookmarkButtonTest : public CocoaTest {
 public:
};

NSEvent* Event(const NSPoint point, const NSEventType type) {
  static NSUInteger eventNumber = 0;  // thx shess
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:0
                        windowNumber:183  // picked out of thin air.
                             context:nil
                         eventNumber:eventNumber++
                          clickCount:1
                            pressure:0.0];
}

// Make sure the basic case of "click" still works.
TEST_F(BookmarkButtonTest, DownUp) {
  scoped_nsobject<NSMutableArray> array;
  array.reset([[NSMutableArray alloc] init]);
  [array addObject:@"foo"];
  [array addObject:@"bar"];

  scoped_nsobject<BookmarkButton> button;
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);

  [button setTarget:array.get()];
  [button setAction:@selector(removeAllObjects)];
  EXPECT_FALSE([[button cell] isHighlighted]);

  NSEvent* downEvent(Event(NSMakePoint(10,10), NSLeftMouseDown));
  NSEvent* upEvent(Event(NSMakePoint(10,10), NSLeftMouseDown));
  [button mouseDown:downEvent];
  EXPECT_TRUE([[button cell] isHighlighted]);
  [button mouseUp:upEvent];
  EXPECT_FALSE([[button cell] isHighlighted]);
  EXPECT_FALSE([array count]);  // confirms target/action fired
}

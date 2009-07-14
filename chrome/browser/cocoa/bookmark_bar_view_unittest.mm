// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkBarViewTest : public testing::Test {
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
};

// Make sure we only get a menu from a right-click.  Not left-click or keyDown.
TEST_F(BookmarkBarViewTest, TestMenu) {
  scoped_nsobject<BookmarkBarView> view([[BookmarkBarView alloc]
                                          initWithFrame:NSMakeRect(0,0,10,10)]);
  EXPECT_TRUE(view.get());

  // Not loaded from a nib so we must set it explicitly
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"spork"]);
  [view setContextMenu:menu.get()];

  EXPECT_TRUE([view menuForEvent:[NSEvent mouseEventWithType:NSRightMouseDown
                                                    location:NSMakePoint(0,0)
                                               modifierFlags:0
                                                   timestamp:0
                                                windowNumber:0
                                                     context:nil
                                                 eventNumber:0
                                                  clickCount:0
                                                    pressure:0.0]]);
  EXPECT_FALSE([view menuForEvent:[NSEvent mouseEventWithType:NSLeftMouseDown
                                                     location:NSMakePoint(0,0)
                                                modifierFlags:0
                                                    timestamp:0
                                                 windowNumber:0
                                                      context:nil
                                                  eventNumber:0
                                                   clickCount:0
                                                     pressure:0.0]]);
  EXPECT_FALSE([view menuForEvent:[NSEvent keyEventWithType:NSKeyDown
                                                   location:NSMakePoint(0,0)
                                              modifierFlags:0
                                                  timestamp:0
                                               windowNumber:0
                                                    context:nil
                                                 characters:@"x"
                                charactersIgnoringModifiers:@"x"
                                                  isARepeat:NO
                                                    keyCode:7]]);

  [view setContextMenu:nil];
}




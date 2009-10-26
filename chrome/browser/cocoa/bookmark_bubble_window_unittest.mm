// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/bookmark_bubble_window.h"

class BookmarkBubbleWindowTest : public CocoaTest {
};

TEST_F(BookmarkBubbleWindowTest, Basics) {
  BookmarkBubbleWindow* window =
      [[BookmarkBubbleWindow alloc] initWithContentRect:NSMakeRect(0, 0, 10, 10)
                                              styleMask:NSBorderlessWindowMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
  EXPECT_TRUE([window canBecomeKeyWindow]);
  EXPECT_FALSE([window canBecomeMainWindow]);

  EXPECT_TRUE([window isExcludedFromWindowsMenu]);
  [window close];
}



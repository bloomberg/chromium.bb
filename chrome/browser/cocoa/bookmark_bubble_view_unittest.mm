// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bubble_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkBubbleViewTest : public PlatformTest {
 public:
  BookmarkBubbleViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    view_.reset([[BookmarkBubbleView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<BookmarkBubbleView> view_;
};

// Test drawing and an add/remove from the view hierarchy to ensure
// nothing leaks or crashes.
TEST_F(BookmarkBubbleViewTest, AddRemoveDisplay) {
  [view_ display];
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

}  // namespace

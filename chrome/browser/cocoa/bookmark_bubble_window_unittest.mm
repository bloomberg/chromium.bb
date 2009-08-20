// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/bookmark_bubble_window.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkBubbleWindowTest : public testing::Test {
 public:
  CocoaTestHelper cocoa_helper_;
};

TEST_F(BookmarkBubbleWindowTest, Basics) {
  scoped_nsobject<BookmarkBubbleWindow> window_;
  window_.reset([[BookmarkBubbleWindow alloc]
                  initWithContentRect:NSMakeRect(0,0,10,10)]);

  EXPECT_TRUE([window_ canBecomeKeyWindow]);
  EXPECT_FALSE([window_ canBecomeMainWindow]);

  EXPECT_TRUE([window_ isExcludedFromWindowsMenu]);
  EXPECT_FALSE([window_ isReleasedWhenClosed]);
}



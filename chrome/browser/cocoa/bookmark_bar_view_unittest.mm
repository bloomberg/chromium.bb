// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkBarViewTest : public PlatformTest {
 public:
  CocoaTestHelper cocoa_helper_;
  scoped_nsobject<BookmarkBarView> view_;
};

// This class only needs to do one thing: prevent mouse down events from moving
// the parent window around.
TEST_F(BookmarkBarViewTest, CanDragWindow) {
  view_.reset([[BookmarkBarView alloc] init]);
  EXPECT_FALSE([view_.get() mouseDownCanMoveWindow]);
}

}  // namespace

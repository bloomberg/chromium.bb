// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_view.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BaseViewTest : public CocoaTest {
 public:
  BaseViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 100);
    scoped_nsobject<BaseView> view([[BaseView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  BaseView* view_;  // weak
};

TEST_VIEW(BaseViewTest, view_)

// Convert a rect in |view_|'s Cocoa coordinate system to gfx::Rect's top-left
// coordinate system. Repeat the process in reverse and make sure we come out
// with the original rect.
TEST_F(BaseViewTest, flipNSRectToRect) {
  NSRect convert = NSMakeRect(10, 10, 50, 50);
  gfx::Rect converted = [view_ flipNSRectToRect:convert];
  EXPECT_EQ(converted.x(), 10);
  EXPECT_EQ(converted.y(), 40);  // Due to view being 100px tall.
  EXPECT_EQ(converted.width(), convert.size.width);
  EXPECT_EQ(converted.height(), convert.size.height);

  // Go back the other way.
  NSRect back_again = [view_ flipRectToNSRect:converted];
  EXPECT_EQ(back_again.origin.x, convert.origin.x);
  EXPECT_EQ(back_again.origin.y, convert.origin.y);
  EXPECT_EQ(back_again.size.width, convert.size.width);
  EXPECT_EQ(back_again.size.height, convert.size.height);
}

}  // namespace

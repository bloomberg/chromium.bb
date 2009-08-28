// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/background_tile_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BackgroundTileViewTest : public testing::Test {
 public:
  BackgroundTileViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    view_.reset([[BackgroundTileView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<BackgroundTileView> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(BackgroundTileViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BackgroundTileViewTest, Display) {
  // Without image set.
  [view_ display];
  // And now with an image.
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [view_ setTileImage:image];
  [view_ display];
}

}  // namespace

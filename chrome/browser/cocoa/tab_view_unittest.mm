// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TabViewTest : public CocoaTest {
 public:
  TabViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<TabView> view([[TabView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  TabView* view_;
};

TEST_VIEW(TabViewTest, view_)

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(TabViewTest, Display) {
  [view_ setHoverAlpha:0.0];
  [view_ display];
  [view_ setHoverAlpha:0.5];
  [view_ display];
  [view_ setHoverAlpha:1.0];
  [view_ display];
}

// Test dragging and mouse tracking.
TEST_F(TabViewTest, MouseTracking) {
  // TODO(pinkerton): Test dragging out of window
}

// Test it doesn't crash when asked for its menu with no TabController set.
TEST_F(TabViewTest, Menu) {
  EXPECT_FALSE([view_ menu]);
}

}  // namespace

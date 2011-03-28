// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
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
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      [view_ setHoverAlpha:i*0.2];
      [view_ setAlertAlpha:j*0.2];
      [view_ display];
    }
  }
}

// Test it doesn't crash when asked for its menu with no TabController set.
TEST_F(TabViewTest, Menu) {
  EXPECT_FALSE([view_ menu]);
}

TEST_F(TabViewTest, Glow) {
  // TODO(viettrungluu): Figure out how to test this, which is timing-sensitive
  // and which moreover uses |-performSelector:withObject:afterDelay:|.

  // Call |-startAlert|/|-cancelAlert| and make sure it doesn't crash.
  for (int i = 0; i < 5; i++) {
    [view_ startAlert];
    [view_ cancelAlert];
  }
  [view_ startAlert];
  [view_ startAlert];
  [view_ cancelAlert];
  [view_ cancelAlert];
}

}  // namespace

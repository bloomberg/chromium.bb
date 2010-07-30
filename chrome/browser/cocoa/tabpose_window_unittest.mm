// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tabpose_window.h"

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabposeWindowTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
};

// Check that this doesn't leak.
TEST_F(TabposeWindowTest, TestShow) {
  scoped_nsobject<NSWindow> parent([[NSWindow alloc]
      initWithContentRect:NSMakeRect(100, 200, 300, 200)
                styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [parent orderFront:nil];
  EXPECT_TRUE([parent isVisible]);

  base::ScopedNSAutoreleasePool pool;
  TabposeWindow* window =
      [TabposeWindow openTabposeFor:parent.get()
                               rect:NSMakeRect(10, 20, 50, 60)
                              slomo:NO];

  // Should release the window.
  [window mouseDown:nil];
}

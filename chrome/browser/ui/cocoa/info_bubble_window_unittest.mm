// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/info_bubble_window.h"

class InfoBubbleWindowTest : public CocoaTest {
};

TEST_F(InfoBubbleWindowTest, Basics) {
  InfoBubbleWindow* window =
      [[InfoBubbleWindow alloc] initWithContentRect:NSMakeRect(0, 0, 10, 10)
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
  EXPECT_TRUE([window canBecomeKeyWindow]);
  EXPECT_FALSE([window canBecomeMainWindow]);

  EXPECT_TRUE([window isExcludedFromWindowsMenu]);
  [window close];
}

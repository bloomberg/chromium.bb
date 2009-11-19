// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/fullscreen_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class FullscreenWindowTest : public CocoaTest {
};

TEST_F(FullscreenWindowTest, Basics) {
  scoped_nsobject<FullscreenWindow> window_;
  window_.reset([[FullscreenWindow alloc] init]);

  EXPECT_EQ([NSScreen mainScreen], [window_ screen]);
  EXPECT_TRUE([window_ canBecomeKeyWindow]);
  EXPECT_TRUE([window_ canBecomeMainWindow]);
  EXPECT_EQ(NSBorderlessWindowMask, [window_ styleMask]);
  EXPECT_TRUE(NSEqualRects([[NSScreen mainScreen] frame], [window_ frame]));
  EXPECT_FALSE([window_ isReleasedWhenClosed]);
}



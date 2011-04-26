// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/browser_frame_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BrowserFrameViewTest : public PlatformTest {
 public:
  BrowserFrameViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 50);
    // We create NSGrayFrame instead of BrowserFrameView because
    // we are swizzling into NSGrayFrame.
    Class browserFrameClass = NSClassFromString(@"NSGrayFrame");
    view_.reset([[browserFrameClass alloc] initWithFrame:frame]);
  }

  scoped_nsobject<NSView> view_;
};

//  Test to make sure our class modifications were successful.
TEST_F(BrowserFrameViewTest, SuccessfulClassModifications) {
  unsigned int count;
  BOOL foundDrawRectOriginal = NO;

  Method* methods = class_copyMethodList([view_ class], &count);
  for (unsigned int i = 0; i < count; ++i) {
    SEL selector = method_getName(methods[i]);
    if (selector == @selector(drawRectOriginal:)) {
      foundDrawRectOriginal = YES;
    }
  }
  EXPECT_TRUE(foundDrawRectOriginal);
  free(methods);
}

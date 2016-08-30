// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/custom_frame_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class CustomFrameViewTest : public PlatformTest {
 public:
  CustomFrameViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 50);
    // We create NSGrayFrame instead of CustomFrameView because
    // we are swizzling into NSThemeFrame.
    Class customFrameClass = NSClassFromString(@"NSThemeFrame");
    view_.reset([[customFrameClass alloc] initWithFrame:frame]);
  }

  base::scoped_nsobject<NSView> view_;
};

//  Test to make sure our class modifications were successful.
TEST_F(CustomFrameViewTest, SuccessfulClassModifications) {
  // In Yosemite, the fullscreen button replaces the zoom button. We no longer
  // need to swizzle out this AppKit private method.
  if (!base::mac::IsOS10_9())
    return;

  unsigned int count;
  BOOL foundSwizzledMethod = NO;

  Method* methods = class_copyMethodList([view_ class], &count);
  for (unsigned int i = 0; i < count; ++i) {
    SEL selector = method_getName(methods[i]);
    if (selector == @selector(_fullScreenButtonOriginOriginal))
      foundSwizzledMethod = YES;
  }
  EXPECT_TRUE(foundSwizzledMethod);
  free(methods);
}

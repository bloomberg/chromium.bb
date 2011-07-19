// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/first_run_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/debug/debugger.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FirstRunBubbleControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
};

// Check that the bubble doesn't crash or leak.
TEST_F(FirstRunBubbleControllerTest, Init) {
  scoped_nsobject<NSWindow> parent([[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 800, 600)
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
         defer:NO]);
  [parent setReleasedWhenClosed:NO];
  if (base::debug::BeingDebugged())
    [parent.get() orderFront:nil];
  else
    [parent.get() orderBack:nil];

  FirstRunBubbleController* controller = [FirstRunBubbleController
      showForView:[parent.get() contentView]
           offset:NSMakePoint(300, 300)
          profile:helper_.profile()];
  EXPECT_TRUE(controller != nil);
  EXPECT_TRUE([[controller window] isVisible]);
  [parent.get() close];
}

}  // namespace

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_projecting_image_view.h"

#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TabProjectingImageViewTest : public CocoaTest {
 public:
  TabProjectingImageViewTest() {
    scoped_nsobject<NSImage> backgroundImage(
        [[NSImage alloc] initWithSize:NSMakeSize(16, 16)]);
    [backgroundImage lockFocus];
    NSRectFill(NSMakeRect(0, 0, 16, 16));
    [backgroundImage unlockFocus];

    scoped_nsobject<NSImage> projectorImage(
        [[NSImage alloc] initWithSize:NSMakeSize(16, 16)]);
    [projectorImage lockFocus];
    NSRectFill(NSMakeRect(0, 0, 16, 16));
    [projectorImage unlockFocus];

    scoped_nsobject<NSImage> throbImage(
        [[NSImage alloc] initWithSize:NSMakeSize(32, 32)]);
    [throbImage lockFocus];
    NSRectFill(NSMakeRect(0, 0, 32, 32));
    [throbImage unlockFocus];

    scoped_nsobject<TabProjectingImageView> view([[TabProjectingImageView alloc]
              initWithFrame:NSMakeRect(0, 0, 32, 32)
            backgroundImage:backgroundImage
             projectorImage:projectorImage
                 throbImage:throbImage
                 durationMS:20
         animationContainer:NULL]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  MessageLoopForUI message_loop_;  // Needed for ui::ThrobAnimation.
  TabProjectingImageView* view_;
};

TEST_VIEW(TabProjectingImageViewTest, view_)

}  // namespace

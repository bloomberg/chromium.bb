// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/throbbing_image_view.h"

#include "base/message_loop/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ThrobbingImageViewTest : public CocoaTest {
 public:
  ThrobbingImageViewTest() {
    base::scoped_nsobject<NSImage> image(
        [[NSImage alloc] initWithSize:NSMakeSize(16, 16)]);
    [image lockFocus];
    NSRectFill(NSMakeRect(0, 0, 16, 16));
    [image unlockFocus];

    base::scoped_nsobject<ThrobbingImageView> view(
        [[ThrobbingImageView alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)
                                  backgroundImage:image
                                       throbImage:image
                                    throbPosition:kThrobPositionOverlay
                               animationContainer:NULL]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  base::MessageLoopForUI message_loop_;  // Needed for gfx::ThrobAnimation.
  ThrobbingImageView* view_;
};

TEST_VIEW(ThrobbingImageViewTest, view_)

}  // namespace


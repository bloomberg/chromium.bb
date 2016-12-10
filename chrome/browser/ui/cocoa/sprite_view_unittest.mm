// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/sprite_view.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

@interface SpriteView (ExposedForTesting)

- (BOOL)isAnimating;

@end

@implementation SpriteView (ExposedForTesting)

- (BOOL)isAnimating {
  return [imageLayer_ animationForKey:[spriteAnimation_ keyPath]] != nil;
}

@end

namespace {

class SpriteViewTest : public CocoaTest {
 public:
  SpriteViewTest() {
    image_.reset(ResourceBundle::GetSharedInstance()
                     .GetNativeImageNamed(IDR_THROBBER)
                     .CopyNSImage());
    view_.reset([[SpriteView alloc] init]);
    [view_ setImage:image_];
    [[test_window() contentView] addSubview:view_];
  }

  base::scoped_nsobject<NSImage> image_;
  base::scoped_nsobject<SpriteView> view_;
};

TEST_VIEW(SpriteViewTest, view_)

TEST_F(SpriteViewTest, TestViewFrame) {
  NSSize imageSize = [image_ size];
  NSRect frame = [view_ frame];
  EXPECT_EQ(0.0, frame.origin.x);
  EXPECT_EQ(0.0, frame.origin.y);
  EXPECT_EQ(imageSize.height, NSWidth(frame));
  EXPECT_EQ(imageSize.height, NSHeight(frame));
}

TEST_F(SpriteViewTest, StopAnimationOnMiniaturize) {
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582
  EXPECT_TRUE([view_ isAnimating]);

  [test_window() miniaturize:nil];
  EXPECT_FALSE([view_ isAnimating]);

  [test_window() deminiaturize:nil];
  EXPECT_TRUE([view_ isAnimating]);
}

TEST_F(SpriteViewTest,
       StopAnimationOnRemoveFromSuperview) {
  EXPECT_TRUE([view_ isAnimating]);

  [view_ removeFromSuperview];
  EXPECT_FALSE([view_ isAnimating]);

  [[test_window() contentView] addSubview:view_];
  EXPECT_TRUE([view_ isAnimating]);
}

}  // namespace

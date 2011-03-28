// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hover_image_button.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

namespace {

class HoverImageButtonTest : public CocoaTest {
 public:
  HoverImageButtonTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    scoped_nsobject<HoverImageButton> button(
        [[HoverImageButton alloc] initWithFrame:content_frame]);
    button_ = button.get();
    [[test_window() contentView] addSubview:button_];
  }

  virtual void SetUp() {
    CocoaTest::BootstrapCocoa();
  }

  HoverImageButton* button_;
};

// Test mouse events.
TEST_F(HoverImageButtonTest, ImageSwap) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(IDR_HOME);
  NSImage* hover = rb.GetNativeImageNamed(IDR_BACK);
  [button_ setDefaultImage:image];
  [button_ setHoverImage:hover];

  [button_ mouseEntered:nil];
  [button_ drawRect:[button_ frame]];
  EXPECT_EQ([button_ image], hover);
  [button_ mouseExited:nil];
  [button_ drawRect:[button_ frame]];
  EXPECT_EQ([button_ image], image);
}

// Test mouse events.
TEST_F(HoverImageButtonTest, Opacity) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(IDR_HOME);
  [button_ setDefaultImage:image];
  [button_ setDefaultOpacity:0.5];
  [button_ setHoverImage:image];
  [button_ setHoverOpacity:1.0];

  [button_ mouseEntered:nil];
  [button_ drawRect:[button_ frame]];
  EXPECT_EQ([button_ alphaValue], 1.0);
  [button_ mouseExited:nil];
  [button_ drawRect:[button_ frame]];
  EXPECT_EQ([button_ alphaValue], 0.5);
}

}  // namespace

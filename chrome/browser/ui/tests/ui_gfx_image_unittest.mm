// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image.h"
#include "ui/gfx/image_unittest.h"

namespace {

class UiGfxImageTest : public CocoaTest {
};

TEST_F(UiGfxImageTest, CheckColor) {
  gfx::Image image(gfx::test::CreateBitmap());
  [image lockFocus];
  NSColor* color = NSReadPixel(NSMakePoint(10, 10));
  [image unlockFocus];

  // SkBitmapToNSImage returns a bitmap in the calibrated color space (sRGB),
  // while NSReadPixel returns a color in the device color space. Convert back
  // to the calibrated color space before testing.
  color = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

  CGFloat components[4] = { 0 };
  [color getComponents:components];

  EXPECT_GT(components[0], 0.95);
  EXPECT_LT(components[1], 0.05);
  EXPECT_LT(components[2], 0.05);
  EXPECT_GT(components[3], 0.95);
}

TEST_F(UiGfxImageTest, ImageView) {
  scoped_nsobject<NSImageView> image_view(
      [[NSImageView alloc] initWithFrame:NSMakeRect(10, 10, 25, 25)]);
  [[test_window() contentView] addSubview:image_view];
  [test_window() orderFront:nil];

  gfx::Image image(gfx::test::CreateBitmap());
  [image_view setImage:image];
}

}  // namespace

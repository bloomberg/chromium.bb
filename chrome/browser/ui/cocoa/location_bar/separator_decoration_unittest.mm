// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/separator_decoration.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef CocoaTest SeparatorDecorationTest;

TEST_F(SeparatorDecorationTest, GetWidthForSpace) {
  SeparatorDecoration decoration;
  CGFloat width = decoration.GetWidthForSpace(1000, 0);
  EXPECT_LT(0, width);
}

TEST_F(SeparatorDecorationTest, DrawInFrame) {
  NSRect frame = NSMakeRect(0, 0, 100, 100);
  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:frame]);
  [[test_window() contentView] addSubview:view];

  [view lockFocus];
  SeparatorDecoration decoration;
  decoration.DrawInFrame(frame, view);
  [view unlockFocus];
}

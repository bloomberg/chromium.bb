// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/search_token_decoration.h"

#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef CocoaTest SearchTokenDecorationTest;

TEST_F(SearchTokenDecorationTest, GetWidthForSpace) {
  SearchTokenDecoration decoration;

  decoration.SetSearchTokenText(ASCIIToUTF16("short"));
  CGFloat short_width = decoration.GetWidthForSpace(1000, 0);
  EXPECT_LT(0, short_width);

  decoration.SetSearchTokenText(ASCIIToUTF16("longer name"));
  CGFloat long_width = decoration.GetWidthForSpace(1000, 0);
  EXPECT_LT(short_width, long_width);

  CGFloat omit_width = decoration.GetWidthForSpace(1, 0);
  EXPECT_EQ(omit_width, LocationBarDecoration::kOmittedWidth);
}

// Test that the search token is hidden if the omnibox text would overlap.
TEST_F(SearchTokenDecorationTest, AutoCollapse) {
  SearchTokenDecoration decoration;
  decoration.SetSearchTokenText(ASCIIToUTF16("short"));
  CGFloat width = decoration.GetWidthForSpace(1000, 1000);
  EXPECT_EQ(width, LocationBarDecoration::kOmittedWidth);
}

TEST_F(SearchTokenDecorationTest, DrawInFrame) {
  NSRect frame = NSMakeRect(0, 0, 100, 100);
  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:frame]);
  [[test_window() contentView] addSubview:view];

  [view lockFocus];
  SearchTokenDecoration decoration;
  decoration.SetSearchTokenText(ASCIIToUTF16("short"));
  decoration.DrawInFrame(frame, view);
  [view unlockFocus];
}

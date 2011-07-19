// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/background_tile_view.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BackgroundTileViewTest : public CocoaTest {
 public:
  BackgroundTileViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    scoped_nsobject<BackgroundTileView> view([[BackgroundTileView alloc]
                                              initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  BackgroundTileView *view_;
};

TEST_VIEW(BackgroundTileViewTest, view_)

// Test drawing with an Image
TEST_F(BackgroundTileViewTest, DisplayImage) {
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [view_ setTileImage:image];
  [view_ display];
}

}  // namespace

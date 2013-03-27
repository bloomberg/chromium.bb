// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/overlay_separator_view.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

class OverlayBottomSeparatorViewTest : public CocoaTest {
 public:
  OverlayBottomSeparatorViewTest() {
    NSView* contentView = [test_window() contentView];
    bottom_view_.reset([[OverlayBottomSeparatorView alloc]
        initWithFrame:[contentView bounds]]);
    [contentView addSubview:bottom_view_];
  }

 protected:
  scoped_nsobject<OverlayBottomSeparatorView> bottom_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayBottomSeparatorViewTest);
};

TEST_VIEW(OverlayBottomSeparatorViewTest, bottom_view_);

TEST_F(OverlayBottomSeparatorViewTest, PreferredHeight) {
  EXPECT_LT(0, [OverlayBottomSeparatorView preferredHeight]);
}

class OverlayTopSeparatorViewTest : public CocoaTest {
 public:
  OverlayTopSeparatorViewTest() {
    NSView* contentView = [test_window() contentView];
    top_view_.reset(
        [[OverlayTopSeparatorView alloc] initWithFrame:[contentView bounds]]);
    [contentView addSubview:top_view_];
  }

 protected:
  scoped_nsobject<OverlayTopSeparatorView> top_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayTopSeparatorViewTest);
};

TEST_VIEW(OverlayTopSeparatorViewTest, top_view_);

TEST_F(OverlayTopSeparatorViewTest, PreferredHeight) {
  EXPECT_LT(0, [OverlayTopSeparatorView preferredHeight]);
}

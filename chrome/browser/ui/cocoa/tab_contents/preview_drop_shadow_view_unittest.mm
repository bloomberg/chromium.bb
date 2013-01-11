// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/preview_drop_shadow_view.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

class PreviewDropShadowViewTest : public CocoaTest {
 public:
  PreviewDropShadowViewTest() {
    NSView* contentView = [test_window() contentView];
    view_.reset(
        [[PreviewDropShadowView alloc] initWithFrame:[contentView bounds]]);
    [contentView addSubview:view_];
  }

 protected:
  scoped_nsobject<PreviewDropShadowView> view_;
};

TEST_VIEW(PreviewDropShadowViewTest, view_);

TEST_F(PreviewDropShadowViewTest, PreferredHeight) {
  EXPECT_LT(0, [PreviewDropShadowView preferredHeight]);
}

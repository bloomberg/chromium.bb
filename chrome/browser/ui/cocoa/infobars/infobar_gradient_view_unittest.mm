// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"

namespace {

class InfoBarGradientViewTest : public CocoaTest {
 public:
  InfoBarGradientViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    base::scoped_nsobject<InfoBarGradientView> view(
        [[InfoBarGradientView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  InfoBarGradientView* view_;  // Weak. Retained by view hierarchy.
};

TEST_VIEW(InfoBarGradientViewTest, view_);

// Assert that the view is non-opaque, because otherwise we will end
// up with findbar painting issues.
TEST_F(InfoBarGradientViewTest, AssertViewNonOpaque) {
  EXPECT_FALSE([view_ isOpaque]);
}

}  // namespace

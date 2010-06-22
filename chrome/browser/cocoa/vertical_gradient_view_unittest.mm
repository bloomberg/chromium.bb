// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/vertical_gradient_view.h"

namespace {

class VerticalGradientViewTest : public CocoaTest {
 public:
  VerticalGradientViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 27);
    scoped_nsobject<VerticalGradientView> view(
        [[VerticalGradientView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  VerticalGradientView* view_;
};

TEST_VIEW(VerticalGradientViewTest, view_);

} // namespace


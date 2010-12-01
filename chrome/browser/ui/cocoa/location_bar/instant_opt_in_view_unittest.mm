// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/location_bar/instant_opt_in_view.h"

namespace {

class InstantOptInViewTest : public CocoaTest {
 public:
  InstantOptInViewTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    scoped_nsobject<InstantOptInView> view(
        [[InstantOptInView alloc] initWithFrame:content_frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  InstantOptInView* view_;  // Weak. Owned by the view hierarchy.
};

// Tests display, add/remove.
TEST_VIEW(InstantOptInViewTest, view_);

}  // namespace

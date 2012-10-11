// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/ratings_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class RatingsViewTest : public CocoaTest {
 public:
  RatingsViewTest() {
    view_.reset([[RatingsView2 alloc] initWithRating:2]);
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  scoped_nsobject<RatingsView2> view_;
};

TEST_VIEW(RatingsViewTest, view_)

TEST_F(RatingsViewTest, Basic) {
  for (int i = 0; i <= 5; ++i) {
    scoped_nsobject<RatingsView2> view(
        [[RatingsView2 alloc] initWithRating:i]);
    [[test_window() contentView] addSubview:view];
    [view display];
    [view removeFromSuperview];
  }
}

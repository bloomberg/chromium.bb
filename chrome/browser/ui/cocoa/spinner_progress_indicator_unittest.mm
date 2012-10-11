// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/spinner_progress_indicator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class SpinnerProgressIndicatorTest : public CocoaTest {
 public:
  SpinnerProgressIndicatorTest() {
    view_.reset([[SpinnerProgressIndicator alloc] initWithFrame:NSZeroRect]);
    [view_ sizeToFit];
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  scoped_nsobject<SpinnerProgressIndicator> view_;
  // Need for the progress indicator timer.
  MessageLoop message_loop_;
};

TEST_VIEW(SpinnerProgressIndicatorTest, view_)

// Test determinate.
TEST_F(SpinnerProgressIndicatorTest, Determinate) {
  [view_ setIsIndeterminate:NO];
  [view_ setPercentDone:0];
  [view_ display];
  [view_ setPercentDone:50];
  [view_ display];
}

// Test indeterminate.
TEST_F(SpinnerProgressIndicatorTest, Indeterminate) {
  [view_ setIsIndeterminate:YES];
  [view_ display];
}

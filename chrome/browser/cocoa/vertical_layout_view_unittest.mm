// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/vertical_layout_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class VerticalLayoutViewTest : public CocoaTest {
 public:
  VerticalLayoutViewTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VerticalLayoutViewTest);
};

TEST_F(VerticalLayoutViewTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  scoped_nsobject<VerticalLayoutView> view([[VerticalLayoutView alloc]
      initWithFrame:NSMakeRect(0, 0, 10, 10)]);
  EXPECT_TRUE(view.get());
}

}

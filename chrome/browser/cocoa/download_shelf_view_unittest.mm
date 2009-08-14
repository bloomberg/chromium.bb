// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DownloadShelfViewTest : public testing::Test {
 public:
  CocoaTestHelper cocoa_helper_;
  scoped_nsobject<DownloadShelfView> view_;
};

// This class only needs to do one thing: prevent mouse down events from moving
// the parent window around.
TEST_F(DownloadShelfViewTest, CanDragWindow) {
  view_.reset([[DownloadShelfView alloc] init]);
  EXPECT_FALSE([view_.get() mouseDownCanMoveWindow]);
}

}  // namespace

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/disclosure_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class DisclosureViewControllerTest : public CocoaTest {
 public:
  DisclosureViewControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisclosureViewControllerTest);
};

TEST_F(DisclosureViewControllerTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  scoped_nsobject<DisclosureViewController> controller(
      [[DisclosureViewController alloc]
      initWithNibName:@"" bundle:nil disclosure:NSOnState]);
  EXPECT_TRUE(controller.get());
}

}

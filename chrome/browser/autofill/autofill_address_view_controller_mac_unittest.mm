// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class AutoFillAddressViewControllerTest : public CocoaTest {
 public:
  AutoFillAddressViewControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillAddressViewControllerTest);
};

TEST_F(AutoFillAddressViewControllerTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  AutoFillProfile profile(ASCIIToUTF16("Home"), 0);
  scoped_nsobject<AutoFillAddressViewController> controller(
      [[AutoFillAddressViewController alloc]
          initWithProfile:profile
               disclosure:NSOffState
               controller:nil]);
  EXPECT_TRUE(controller.get());
}

}

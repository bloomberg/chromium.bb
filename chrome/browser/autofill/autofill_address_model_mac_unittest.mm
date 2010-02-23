// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_model_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class AutoFillAddressModelTest : public CocoaTest {
 public:
  AutoFillAddressModelTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillAddressModelTest);
};

TEST_F(AutoFillAddressModelTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  AutoFillProfile profile(ASCIIToUTF16("Home"), 0);
  AutoFillAddressModel* model = [[AutoFillAddressModel alloc]
      initWithProfile:profile];
  [model release];
  ASSERT_TRUE(true);
}

}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_sheet_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef CocoaTest AutoFillAddressSheetControllerTest;

TEST(AutoFillAddressSheetControllerTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  AutoFillProfile profile;
  scoped_nsobject<AutoFillAddressSheetController> controller(
      [[AutoFillAddressSheetController alloc]
          initWithProfile:profile
                     mode:kAutoFillAddressAddMode]);
  EXPECT_TRUE(controller.get());
}

}  // namespace

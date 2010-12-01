// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_sheet_controller_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef CocoaTest AutoFillCreditCardSheetControllerTest;

TEST(AutoFillCreditCardSheetControllerTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  CreditCard credit_card;
  scoped_nsobject<AutoFillCreditCardSheetController> controller(
      [[AutoFillCreditCardSheetController alloc]
          initWithCreditCard:credit_card
                        mode:kAutoFillCreditCardAddMode]);
  EXPECT_TRUE(controller.get());
}

}  // namespace


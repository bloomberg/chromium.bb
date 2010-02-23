// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class AutoFillCreditCardModelTest : public CocoaTest {
 public:
  AutoFillCreditCardModelTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillCreditCardModelTest);
};

TEST_F(AutoFillCreditCardModelTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  CreditCard credit_card(ASCIIToUTF16("myCC"), 0);
  AutoFillCreditCardModel* model = [[AutoFillCreditCardModel alloc]
      initWithCreditCard:credit_card];
  [model release];
  ASSERT_TRUE(true);
}

}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class AutoFillCreditCardViewControllerTest : public CocoaTest {
 public:
  AutoFillCreditCardViewControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillCreditCardViewControllerTest);
};

TEST_F(AutoFillCreditCardViewControllerTest, HelloWorld) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  CreditCard credit_card(ASCIIToUTF16("myCC"), 0);
  scoped_nsobject<AutoFillCreditCardViewController> controller(
      [[AutoFillCreditCardViewController alloc]
          initWithCreditCard:credit_card
                  disclosure:NSOffState
                  controller:nil]);
  EXPECT_TRUE(controller.get());
}

}

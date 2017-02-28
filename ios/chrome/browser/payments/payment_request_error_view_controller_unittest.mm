// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_error_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

class PaymentRequestErrorViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[PaymentRequestErrorViewController alloc] init];
  }

  PaymentRequestErrorViewController* GetPaymentRequestErrorViewController() {
    return base::mac::ObjCCastStrict<PaymentRequestErrorViewController>(
        controller());
  }
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentRequestErrorViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_PAYMENTS_TITLE);

  [GetPaymentRequestErrorViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be one item for the error message.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // They item should be of type PriceItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
}

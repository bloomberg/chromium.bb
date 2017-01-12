// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShippingOptionSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[ShippingOptionSelectionViewController alloc] init];
  }

  ShippingOptionSelectionViewController* ShippingOptionSelectionController() {
    return base::mac::ObjCCastStrict<ShippingOptionSelectionViewController>(
        controller());
  }
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(ShippingOptionSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_SHIPPING_OPTION_SELECTION_TITLE);

  std::unique_ptr<web::PaymentShippingOption> option1(
      new web::PaymentShippingOption());
  std::unique_ptr<web::PaymentShippingOption> option2(
      new web::PaymentShippingOption());

  std::vector<web::PaymentShippingOption*> shippingOptions;
  shippingOptions.push_back(option1.get());
  shippingOptions.push_back(option2.get());

  [ShippingOptionSelectionController() setShippingOptions:shippingOptions];
  [ShippingOptionSelectionController()
      setSelectedShippingOption:shippingOptions[0]];
  [ShippingOptionSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping options.
  EXPECT_EQ(shippingOptions.size(),
            static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first option should appear to be selected.
  CollectionViewTextItem* item = GetCollectionViewItem(0, 0);
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark, item.accessoryType);
  item = GetCollectionViewItem(0, 1);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, item.accessoryType);
}

}  // namespace
